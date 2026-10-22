#include "mpvcontroller_p.h"

void MpvController::Impl::updateProperty(const mpv_event &event) {
    if (updateMediaInformation(event))
        return;
    const auto &value = *static_cast<mpv_event_property *>(event.data);
    if (event.reply_userdata == VideoImage) {
        hasMotionVideo =
            value.data && (state.active || loading) && !*static_cast<int *>(value.data);
        emit owner->stateChanged();
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
            snapshot.append({QStringLiteral("auto"), owner->tr("自动选择")});
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
        emit owner->stateChanged();
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
        emit owner->stateChanged();
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
    emit owner->stateChanged();
}

void MpvController::Impl::drainEvents() {
    if (shuttingDown)
        return;
    while (const auto *event = mpv_wait_event(client.get(), 0)) {
        switch (event->event_id) {
        case MPV_EVENT_NONE:
            return;
        case MPV_EVENT_START_FILE:
            playingEntry = static_cast<mpv_event_start_file *>(event->data)->playlist_entry_id;
            commitPendingOpen(playingEntry);
            resetPlayback();
            loading = true;
            if (openGenerationsByEntry.value(playingEntry) == openGeneration)
                pendingPlayback.reset();
            emit owner->stateChanged();
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
            queryChapterRequest(chapterSeekStarted);
            break;
        case MPV_EVENT_COMMAND_REPLY:
            handleOpeningCommandReply(*event);
            confirmChapterRequest(*event);
            if (canceledRestoreRequests.remove(event->reply_userdata))
                break;
            if (event->reply_userdata == restoreCommand && restoreCommand) {
                restoreCommand = 0;
                restoreNextSubtitle();
            }
            report(event->error);
            break;
        case MPV_EVENT_GET_PROPERTY_REPLY:
            if (finishInformationQuery(*event))
                break;
            if (finishChapterQuery(*event))
                break;
            if (finishTuningQuery(*event))
                break;
            if (canceledRestoreRequests.remove(event->reply_userdata))
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
            emit owner->stateChanged();
            return;
        default:
            break;
        }
    }
}

void MpvController::Impl::handleEndFile(const mpv_event_end_file &end) {
    if (!openGenerationsByEntry.contains(end.playlist_entry_id) &&
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
    const bool expectedReset = recoveringRenderer && end.error == MPV_ERROR_VO_INIT_FAILED;
    if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
        recoveringRenderer = false;
        recovery.reset();
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
    if (end.reason == MPV_END_FILE_REASON_EOF)
        emit owner->playbackFinished();
}

void MpvController::Impl::handleFileLoaded() {
    if (startedEntries.contains(playingEntry) && !openGenerationsByEntry.contains(playingEntry)) {
        loadedEntries.insert(playingEntry);
        return;
    }
    if (openGenerationsByEntry.contains(playingEntry) &&
        openGenerationsByEntry.value(playingEntry) != openGeneration)
        return;
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
