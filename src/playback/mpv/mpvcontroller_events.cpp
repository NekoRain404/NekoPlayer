#include "playback/media/mediasource.h"
#include "playback/mpv/mpvcontroller_p.h"

#include <QCoreApplication>

void MpvController::Impl::updateProperty(const mpv_event &event) {
    if (updateMediaInformation(event))
        return;
    const auto &value = *static_cast<mpv_event_property *>(event.data);
    if (event.reply_userdata == VideoImage) {
        hasMotionVideo =
            value.data && (state.active || loading) && !*static_cast<int *>(value.data);
        stateDirty = true;
        return;
    }
    if (event.reply_userdata == Chapters) {
        updateChapters(value.data && (state.active || loading)
                           ? parseMpvChapters(*static_cast<mpv_node *>(value.data))
                           : QList<MediaChapter>{});
        return;
    }
    if (event.reply_userdata == Tracks) {
        updateTracks(value.data && (state.active || loading)
                         ? parseMpvTracks(*static_cast<mpv_node *>(value.data))
                         : QList<MediaTrack>{});
        return;
    }
    if (event.reply_userdata == AudioDevices) {
        QList<AudioDevice> snapshot;
        if (value.data)
            snapshot = parseMpvAudioDevices(*static_cast<mpv_node *>(value.data));
        else
            snapshot.append(
                {QStringLiteral("auto"), QCoreApplication::translate("AudioDevice", "自动选择")});
        if (audioDevices == snapshot)
            return;
        audioDevices = snapshot;
        emit owner->audioDevicesChanged();
        return;
    }
    if (event.reply_userdata == SelectedAudioDevice) {
        const auto selected = value.data ? QString::fromUtf8(*static_cast<char **>(value.data))
                                         : QStringLiteral("auto");
        if (selectedAudioDevice == selected)
            return;
        selectedAudioDevice = selected;
        stateDirty = true;
        return;
    }
    if (!value.data) {
        switch (event.reply_userdata) {
        case Position:
            state.position = 0;
            break;
        case Duration:
            state.duration = 0;
            break;
        case Seekable:
            state.seekable = false;
            break;
        case HardwareDecoder:
            state.hardwareDecoder.clear();
            break;
        case Chapter:
            state.currentChapter = -1;
            break;
        default:
            return;
        }
        stateDirty = true;
        return;
    }
    switch (event.reply_userdata) {
    case Chapter:
        observeChapter(*static_cast<int64_t *>(value.data));
        break;
    case Speed:
        observeTuning(speed, *static_cast<double *>(value.data));
        return;
    case AudioDelay:
        observeTuning(audioDelay, *static_cast<double *>(value.data));
        return;
    case SubtitleDelay:
        observeTuning(subtitleDelay, *static_cast<double *>(value.data));
        return;
    case Position:
        if (state.active) {
            state.position = *static_cast<double *>(value.data);
            if (mediaReady && !openingCommand && playingEntry == openingEntry)
                openingPosition.reset();
        }
        break;
    case Duration:
        if (state.active)
            state.duration = *static_cast<double *>(value.data);
        break;
    case Paused:
        state.paused = *static_cast<int *>(value.data) != 0;
        break;
    case Volume:
        state.volume = *static_cast<double *>(value.data);
        break;
    case Muted:
        state.muted = *static_cast<int *>(value.data) != 0;
        break;
    case Seekable:
        state.seekable = state.active && *static_cast<int *>(value.data) != 0;
        break;
    case HardwareDecoder:
        state.hardwareDecoder = QString::fromUtf8(*static_cast<char **>(value.data));
        break;
    default:
        return;
    }
    stateDirty = true;
}

void MpvController::Impl::drainEvents() {
    if (shuttingDown)
        return;
    while (const auto *event = mpv_wait_event(client.get(), 0)) {
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            flushStateChanged();
            return;
        case MPV_EVENT_START_FILE:
            handleStartFile(static_cast<mpv_event_start_file *>(event->data)->playlist_entry_id);
            break;
        case MPV_EVENT_FILE_LOADED:
            handleFileLoaded();
            break;
        case MPV_EVENT_END_FILE: {
            const auto &end = *static_cast<mpv_event_end_file *>(event->data);
            handleEndFile(end);
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE:
            updateProperty(*event);
            break;
        case MPV_EVENT_AUDIO_RECONFIG:
        case MPV_EVENT_VIDEO_RECONFIG:
            scheduleInformationRefresh();
            break;
        case MPV_EVENT_SEEK:
            chapterSeekStarted = requestedChapter.has_value() && !chapterCommand;
            break;
        case MPV_EVENT_PLAYBACK_RESTART:
            scheduleInformationRefresh();
            queryChapterRequest(chapterSeekStarted);
            break;
        case MPV_EVENT_COMMAND_REPLY:
            if (event->reply_userdata == pendingRecoveryRestore) {
                settleDeferredRecovery();
                break;
            }
            if (canceledCommands.remove(event->reply_userdata)) {
                // A screenshot canceled by stop() still completes on its own reply;
                // only the error must stay out of the user-facing error stream.
                if (screenshotCommand && event->reply_userdata == screenshotCommand) {
                    const auto path = std::exchange(screenshotPath, {});
                    const bool success = event->error >= 0;
                    screenshotCommand = 0;
                    emit owner->screenshotPendingChanged();
                    emit owner->screenshotFinished(path, success);
                }
                break;
            }
            if (finishScreenshot(*event))
                break;
            handleOpeningCommandReply(*event);
            confirmChapterRequest(*event);
            if (canceledRestoreRequests.remove(event->reply_userdata))
                break;
            if (finishPreferenceCommand(*event))
                break;
            if (event->reply_userdata == restoreCommand && restoreCommand) {
                restoreCommand = 0;
                restoreNextExternalTrack();
            }
            report(event->error);
            break;
        case MPV_EVENT_GET_PROPERTY_REPLY:
            if (event->reply_userdata == pendingRecoveryRestore) {
                settleDeferredRecovery();
                break;
            }
            if (finishInformationQuery(*event))
                break;
            if (finishChapterQuery(*event))
                break;
            if (finishTuningQuery(*event))
                break;
            if (canceledRestoreRequests.remove(event->reply_userdata))
                break;
            if (finishPreferenceQuery(*event))
                break;
            if (event->reply_userdata == restoreQuery && restoreQuery) {
                restoreQuery = 0;
                const auto &property = *static_cast<mpv_event_property *>(event->data);
                if (event->error >= 0 && property.data)
                    restoreSelections(parseMpvTracks(*static_cast<mpv_node *>(property.data)));
                else
                    cancelTrackRestore();
            }
            report(event->error);
            break;
        case MPV_EVENT_SET_PROPERTY_REPLY:
            confirmAudioDevice(*event);
            confirmTuning(*event);
            report(event->error);
            break;
        case MPV_EVENT_SHUTDOWN:
            ready = false;
            openingPosition.reset();
            openingCommand = 0;
            recoveringRenderer = false;
            resetPlayback();
            stateDirty = false;
            emit owner->stateChanged();
            return;
        default:
            break;
        }
    }
    flushStateChanged();
}

void MpvController::Impl::handleStartFile(int64_t entry) {
    playingEntry = entry;
    // A recovery load is not registered in pendingOpens. Claim its entry here
    // when the COMMAND_REPLY has not arrived yet, so a terminal event can be
    // attributed even if it precedes the reply. Entries already owned by a
    // normal open are left to commitPendingOpen's correlation.
    if (recoveryLoad && !recoveryLoad->entryKnown && pendingOpens.isEmpty() &&
        !openGenerationsByEntry.contains(entry) && !startedEntries.contains(entry)) {
        recoveryLoad->entry = entry;
        recoveryLoad->entryKnown = true;
    }
    commitPendingOpen(entry);
    resetPlayback();
    loading = true;
    if (openGenerationsByEntry.value(entry) == openGeneration)
        pendingPlayback.reset();
    emit owner->stateChanged();
}

void MpvController::Impl::handleEndFile(const mpv_event_end_file &end) {
    if (invalidatedEntries.contains(end.playlist_entry_id)) {
        // stop() invalidated this entry. Its terminal event is still the point
        // where the playback snapshot is cleared, but it must not report EOF or
        // a load failure for a stopped file.
        resetPlayback();
        pendingPlayback.reset();
        emit owner->stateChanged();
        return;
    }
    // Only a correlated entry may be attributed to the recovery reload. An
    // uncorrelated END_FILE belongs to the superseded load or to the released
    // renderer, and is swallowed by the expectedReset check below.
    const bool recoveryEntry =
        recoveryLoad && recoveryLoad->entryKnown && recoveryLoad->entry == end.playlist_entry_id;
    if (!recoveryEntry && !openGenerationsByEntry.contains(end.playlist_entry_id) &&
        startedEntries.contains(end.playlist_entry_id)) {
        endedEntries.insert(end.playlist_entry_id, end);
        return;
    }
    const bool staleOpen = openGenerationsByEntry.contains(end.playlist_entry_id) &&
                           openGenerationsByEntry.value(end.playlist_entry_id) != openGeneration;
    if (staleOpen) {
        openGenerationsByEntry.remove(end.playlist_entry_id);
        startedEntries.remove(end.playlist_entry_id);
        loadedEntries.remove(end.playlist_entry_id);
        return;
    }
    const auto generation = openGenerationsByEntry.value(end.playlist_entry_id, openGeneration);
    if (end.playlist_entry_id == openingEntry)
        openingPosition.reset();
    openGenerationsByEntry.remove(end.playlist_entry_id);
    startedEntries.remove(end.playlist_entry_id);
    loadedEntries.remove(end.playlist_entry_id);
    if (recoveryEntry) {
        // The tracked recovery reload itself ended. Distinguish its failure
        // from the renderer teardown that initially triggered recovery.
        if (end.reason == MPV_END_FILE_REASON_ERROR) {
            failRecoveryLoad(end.error, generation);
            if (generation != openGeneration)
                return;
            report(end.error);
        } else {
            finishRecoveryLoad();
            resetPlayback();
            pendingPlayback.reset();
            if (generation != openGeneration)
                return;
            emit owner->stateChanged();
            if (end.reason == MPV_END_FILE_REASON_EOF)
                emit owner->playbackFinished();
        }
        return;
    }
    // A released renderer ends the old entry with VO_INIT_FAILED. While a newer
    // recovery reload is in flight, terminal events for other entries also
    // belong to superseded loads and must not cancel the retry.
    const bool expectedReset =
        recoveringRenderer && (end.error == MPV_ERROR_VO_INIT_FAILED || recoveryLoad.has_value());
    if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
        recoveringRenderer = false;
        recovery.reset();
        recoveryLoad.reset();
    }
    resetPlayback();
    pendingPlayback.reset();
    if (generation != openGeneration)
        return;
    emit owner->stateChanged();
    if (generation != openGeneration)
        return;
    if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
        emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(end.error)));
        if (generation != openGeneration)
            return;
        report(end.error);
    }
    // A superseded entry ending while the recovery reload is still in flight
    // must not advance the session queue.
    if (end.reason == MPV_END_FILE_REASON_EOF && !recoveryLoad)
        emit owner->playbackFinished();
}

void MpvController::Impl::handleFileLoaded() {
    // stop() clears playingEntry and tombstones the previous entries. A stale
    // FILE_LOADED must not resurrect playback or notify about the wrong file.
    if (playingEntry < 0 || invalidatedEntries.contains(playingEntry))
        return;
    if (startedEntries.contains(playingEntry) && !openGenerationsByEntry.contains(playingEntry)) {
        loadedEntries.insert(playingEntry);
        return;
    }
    if (openGenerationsByEntry.contains(playingEntry) &&
        openGenerationsByEntry.value(playingEntry) != openGeneration)
        return;
    // FILE_LOADED carries no entry id, so match the tracked recovery entry. If
    // its COMMAND_REPLY is still pending, a START_FILE claimed while no normal
    // open existed stands in for the correlation.
    if (recoveryLoad &&
        (recoveryLoad->entryKnown
             ? recoveryLoad->entry == playingEntry
             : (pendingOpens.isEmpty() && !openGenerationsByEntry.contains(playingEntry) &&
                !startedEntries.contains(playingEntry))))
        finishRecoveryLoad();
    const auto generation = openGeneration;
    loading = false;
    recoveringRenderer = false;
    state.active = true;
    mediaReady = true;
    error.clear();
    scheduleInformationRefresh();
    restoreExternalTracks();
    emit owner->stateChanged();
    if (generation != openGeneration || (openGenerationsByEntry.contains(playingEntry) &&
                                         openGenerationsByEntry.value(playingEntry) != generation))
        return;
    emit owner->fileLoaded();
}

void MpvController::Impl::finishRecoveryLoad() {
    recoveryLoad.reset();
    recovery.reset();
    recoveryAttempts = 0;
    recoveringRenderer = false;
}

void MpvController::Impl::failRecoveryLoad(int status, uint64_t generation) {
    recoveryLoad.reset();
    // Let resetPlayback() clear media-local state (AB loop, chapters, tracks);
    // resetAbLoop() is suppressed while recoveringRenderer is set.
    recoveringRenderer = false;
    openingCommand = 0;
    openingPosition.reset();
    if (recoveryAttempts >= maxRecoveryAttempts) {
        // Retry budget exhausted: drop the snapshot so a later loss opens with
        // a fresh state instead of replaying media that already failed twice.
        recovery.reset();
        recoveryAttempts = 0;
        cancelTrackRestore();
    }
    resetPlayback();
    pendingPlayback.reset();
    // A reentrant open supersedes this failure, exactly like the normal END_FILE
    // path; do not report the old media or clobber the newer request.
    if (generation != openGeneration)
        return;
    emit owner->stateChanged();
    if (generation != openGeneration)
        return;
    emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(status)));
}

void MpvController::Impl::settleDeferredRecovery() {
    canceledRestoreRequests.remove(pendingRecoveryRestore);
    pendingRecoveryRestore = 0;
    if (!recoveryResumePending)
        return;
    recoveryResumePending = false;
    owner->resumeAfterRendererReset();
}

void MpvController::prepareForRendererReset() {
    auto &self = *impl_;
    const auto restoreReply = self.restoreQuery ? self.restoreQuery : self.restoreCommand;
    if (restoreReply) {
        // Preserve the intended tracks before cancelTrackRestore() clears the
        // working set, then defer the reload until the backend replies.
        if (!self.tracksToRestore.isEmpty())
            self.pendingRestoreTracks = self.tracksToRestore;
        self.pendingRecoveryRestore = restoreReply;
        self.recoveryResumePending = true;
        self.cancelTrackRestore();
    }
    if (self.recovery) {
        // A second context loss before the retained snapshot has been replayed
        // must not replace it with the half-reset state. Keep the snapshot and
        // refresh only the load intent that is newer than it.
        if (self.openingPosition)
            self.recovery->playback.position = *self.openingPosition;
        if (!self.pendingRestoreTracks.isEmpty())
            self.recovery->tracks = self.pendingRestoreTracks;
        else if (!self.tracksToRestore.isEmpty())
            self.recovery->tracks = self.tracksToRestore;
        // The in-flight reload died with this renderer. Tombstone its entry and
        // drop the registration so its teardown is not reported as a failure;
        // the next ready submits a fresh attempt from the retained snapshot.
        if (self.recoveryLoad) {
            if (self.recoveryLoad->entryKnown)
                self.invalidateEntry(self.recoveryLoad->entry);
            if (self.recoveryLoad->command) {
                self.canceledCommands.insert(self.recoveryLoad->command);
                if (self.openingCommand == self.recoveryLoad->command)
                    self.openingCommand = 0;
            }
            self.recoveryLoad.reset();
        }
        self.recoveringRenderer = true;
        return;
    }
    // A context loss while a file is still loading must also be recoverable.
    // Releasing the renderer disables video, and without a snapshot the reload
    // never happens; only a fully idle controller keeps no snapshot.
    if (!state().active && !self.loading && !self.pendingPlayback)
        return;
    self.recovery = Impl::RecoveryState{
        state(),
        self.tracksToRestore.isEmpty()
            ? (self.pendingRestoreTracks.isEmpty() ? tracks() : self.pendingRestoreTracks)
            : self.tracksToRestore,
        self.explicitExternalPaths};
    if (self.openingPosition)
        self.recovery->playback.position = *self.openingPosition;
    self.recoveringRenderer = true;
}

void MpvController::resumeAfterRendererReset() {
    auto &self = *impl_;
    if (!self.recovery)
        return;
    // A duplicate ready while the reload is still in flight must not load the
    // file twice. A failed attempt clears this registration so the retained
    // snapshot can be retried.
    if (self.recoveryLoad)
        return;
    if (self.pendingRecoveryRestore) {
        // Do not race the superseded sub-add/audio-add worker; the reply handler
        // calls this function again once the old restore has settled.
        self.recoveryResumePending = true;
        return;
    }
    const auto saved = *self.recovery;
    const auto generation = self.openGeneration;
    const auto identity = self.restoreIdentity + 1;
    self.cancelTrackRestore();
    if (generation != self.openGeneration || identity != self.restoreIdentity)
        return;
    const auto selectedInternal = [&saved](MediaTrack::Type type) {
        for (const auto &track : saved.tracks) {
            if (track.type == type && track.selected && !track.external)
                return QByteArray::number(track.id);
        }
        return QByteArray("no");
    };
    // Releasing an mpv renderer disables video. Reload at the saved position,
    // including video-only files whose core may have reached END_FILE.
    // Recovery overrides the session's sub-auto setting with an explicit no: the
    // saved external set is restored below, so auto-loading would add duplicates.
    // This file-local option outlives the property, so toggling auto-load while
    // the recovered file plays takes effect on the next normal load, not a retry.
    const auto loopPoint = [](const std::optional<double> &value) {
        return value ? QByteArray::number(*value, 'g', 17) : QByteArray("no");
    };
    const auto position = MediaSource::isNetwork(saved.playback.path) && !saved.playback.seekable
                              ? 0.0
                              : saved.playback.position;
    const auto options =
        QByteArray("start=") + QByteArray::number(position, 'f', 3) +
        ",pause=" + (saved.playback.paused ? "yes" : "no") +
        ",aid=" + selectedInternal(MediaTrack::Type::Audio) +
        ",sid=" + selectedInternal(MediaTrack::Type::Subtitle) + ",sub-auto=no,audio-file-auto=no" +
        ",ab-loop-a=" + loopPoint(abLoop().start) + ",ab-loop-b=" + loopPoint(abLoop().end);
    self.tracksToRestore = saved.tracks;
    self.pendingRestoreTracks.clear();
    self.state.path = saved.playback.path;
    self.playingPath = saved.playback.path;
    self.openingPosition = position;
    self.openingEntry = -1;
    // Reset suppresses the AB-loop clear while a recovery is being replayed.
    self.recoveringRenderer = true;
    self.resetPlayback();
    self.explicitExternalPathsToRestore = saved.explicitExternalPaths;
    emit stateChanged();
    if (generation != self.openGeneration || identity != self.restoreIdentity)
        return;
    ++self.recoveryAttempts;
    const auto [id, status] =
        self.submitCommand({"loadfile", saved.playback.path.toUtf8(), "replace", "-1", options});
    self.openingCommand = id;
    if (id) {
        self.recoveryLoad = Impl::RecoveryLoad{};
        self.recoveryLoad->command = id;
    } else {
        // No END_FILE follows a rejected submission; treat it as the attempt's
        // terminal event so the retry budget and error signal still apply.
        self.failRecoveryLoad(status, generation);
    }
    self.report(status);
}
