#include "mediasource.h"
#include "mpvcontroller_p.h"

MpvController::MpvController(bool headless, bool silentAudio, QObject *parent)
    : QObject(parent), impl_(std::make_unique<Impl>(this)) {
    auto &backend = *impl_;
    if (!backend.client) {
        backend.error = QStringLiteral("无法创建 libmpv 实例");
        return;
    }
    constexpr auto options = std::array{
        std::pair{"config", "no"}, std::pair{"load-scripts", "no"}, std::pair{"terminal", "no"},
        std::pair{"osc", "no"}, std::pair{"input-default-bindings", "no"},
        std::pair{"input-vo-keyboard", "no"}, std::pair{"hwdec", "auto-safe"},
        std::pair{"idle", "yes"}, std::pair{"audio-pitch-correction", "yes"},
        std::pair{"deinterlace", "auto"}, std::pair{"reset-on-next-file", "ab-loop-a,ab-loop-b"},
        std::pair{"keep-open", "no"}, std::pair{"video-timing-offset", "0"},
        // Sessions may enable exact sibling matching; direct/headless controller
        // use must not auto-load subtitles implicitly.
        std::pair{"sub-auto", "no"},
        // Eight-tap kernels avoid mpv 0.41's uninitialized LUT padding.
        std::pair{"scale", "spline64"}, std::pair{"cscale", "spline64"},
        std::pair{"dscale", "spline64"}};
    for (const auto &[name, value] : options) {
        if (!backend.option(name, value))
            return;
    }
    if (!backend.option("vo", headless ? "null" : "libmpv"))
        return;
    if ((headless || silentAudio) && !backend.option("ao", "null"))
        return;
    const int status = mpv_initialize(backend.client.get());
    backend.report(status);
    if (status < 0)
        return;
    const auto observations =
        std::array{std::tuple{Impl::Position, "time-pos", MPV_FORMAT_DOUBLE},
                   std::tuple{Impl::Duration, "duration", MPV_FORMAT_DOUBLE},
                   std::tuple{Impl::Paused, "pause", MPV_FORMAT_FLAG},
                   std::tuple{Impl::Volume, "volume", MPV_FORMAT_DOUBLE},
                   std::tuple{Impl::Muted, "mute", MPV_FORMAT_FLAG},
                   std::tuple{Impl::Seekable, "seekable", MPV_FORMAT_FLAG},
                   std::tuple{Impl::VideoImage, "current-tracks/video/image", MPV_FORMAT_FLAG},
                   std::tuple{Impl::HardwareDecoder, "hwdec-current", MPV_FORMAT_STRING},
                   std::tuple{Impl::Tracks, "track-list", MPV_FORMAT_NODE},
                   std::tuple{Impl::AudioDevices, "audio-device-list", MPV_FORMAT_NODE},
                   std::tuple{Impl::SelectedAudioDevice, "audio-device", MPV_FORMAT_STRING},
                   std::tuple{Impl::Chapters, "chapter-list", MPV_FORMAT_NODE},
                   std::tuple{Impl::Chapter, "chapter", MPV_FORMAT_INT64},
                   std::tuple{Impl::Speed, "speed", MPV_FORMAT_DOUBLE},
                   std::tuple{Impl::AudioDelay, "audio-delay", MPV_FORMAT_DOUBLE},
                   std::tuple{Impl::SubtitleDelay, "sub-delay", MPV_FORMAT_DOUBLE}};
    for (const auto &[id, name, format] : observations) {
        const int result = mpv_observe_property(backend.client.get(), id, name, format);
        backend.report(result);
        if (result < 0)
            return;
    }
    for (const auto &[id, name, format] : MpvPrivate::informationProperties) {
        const int result = mpv_observe_property(backend.client.get(), id, name, format);
        backend.report(result);
        if (result < 0)
            return;
    }
    backend.ready = true;
    // Impl and its QObject receiver remain alive until mpv's callback threads stop.
    mpv_set_wakeup_callback(backend.client.get(), &Impl::wakeup, impl_.get());
}

MpvController::~MpvController() {
    impl_->shuttingDown = true;
    if (impl_->client) {
        mpv_set_wakeup_callback(impl_->client.get(), nullptr, nullptr);
        impl_->client.reset();
    }
}

bool MpvController::isReady() const { return impl_->ready; }
QString MpvController::errorString() const { return impl_->error; }
mpv_handle *MpvController::handle() const { return impl_->client.get(); }
const PlaybackState &MpvController::state() const {
    return impl_->pendingPlayback ? *impl_->pendingPlayback : impl_->state;
}
double MpvController::resumePosition() const {
    if (impl_->openingPosition)
        return *impl_->openingPosition;
    const auto &state = impl_->state;
    if (!impl_->mediaReady || !state.seekable || !std::isfinite(state.position) ||
        state.position <= 0 || state.duration - state.position <= 1.0)
        return 0;
    return state.position;
}
const MediaInformation &MpvController::mediaInformation() const {
    static const MediaInformation empty;
    return impl_->pendingPlayback ? empty : impl_->information;
}
const QList<MediaTrack> &MpvController::tracks() const {
    static const QList<MediaTrack> empty;
    return impl_->pendingPlayback ? empty : impl_->tracks;
}
const QList<MediaChapter> &MpvController::chapters() const {
    static const QList<MediaChapter> empty;
    return impl_->pendingPlayback ? empty : impl_->chapters;
}
bool MpvController::hardwareDecodingEnabled() const { return impl_->hardwareDecodingEnabled; }

void MpvController::Impl::commitPendingOpen(int64_t entry, bool alreadyStarted) {
    auto found = std::find_if(pendingOpens.begin(), pendingOpens.end(),
                              [entry](const PendingOpen &open) { return open.entry == entry; });
    if (found == pendingOpens.end()) {
        if (std::any_of(pendingOpens.cbegin(), pendingOpens.cend(),
                        [](const PendingOpen &open) { return !open.replyReceived; }))
            startedEntries.insert(entry);
        return;
    }
    const auto opened = *found;
    pendingOpens.erase(found);
    if (opened.generation == openGeneration) {
        openingEntry = entry;
        state.path = opened.path;
    }
    playingPath = opened.path;
    openGenerationsByEntry.insert(entry, opened.generation);
    if (alreadyStarted)
        return;
    mediaReady = false;
    cancelInformationQueries();
    const bool informationChanged = information != MediaInformation{};
    information = {};
    cancelChapterRequest();
    recovery.reset();
    cancelTrackRestore();
    recoveringRenderer = false;
    resetAbLoop();
    property("pause", MPV_FORMAT_FLAG, 0);
    if (informationChanged)
        emit owner->mediaInformationChanged();
}

void MpvController::Impl::rejectPendingOpen(uint64_t command, int status) {
    const auto found =
        std::find_if(pendingOpens.begin(), pendingOpens.end(),
                     [command](const PendingOpen &open) { return open.command == command; });
    if (found == pendingOpens.end())
        return;
    const auto generation = found->generation;
    pendingOpens.erase(found);
    if (openingCommand == command)
        openingCommand = 0;
    if (generation == openGeneration) {
        state.path = playingPath;
        openingPosition.reset();
        openingEntry = -1;
        pendingPlayback.reset();
        emit owner->stateChanged();
        if (generation != openGeneration)
            return;
        if (information != MediaInformation{})
            emit owner->mediaInformationChanged();
        if (generation != openGeneration)
            return;
        if (!tracks.isEmpty())
            emit owner->tracksChanged();
        if (generation != openGeneration)
            return;
        if (!chapters.isEmpty())
            emit owner->chaptersChanged();
        if (generation != openGeneration)
            return;
        if (abLoop != AbLoop{})
            emit owner->abLoopChanged();
        if (generation != openGeneration)
            return;
        emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(status)));
    }
}

void MpvController::Impl::notifySynchronousOpenFailure(uint64_t generation, int status) {
    if (generation != openGeneration)
        return;
    state.path = playingPath;
    openingPosition.reset();
    openingEntry = -1;
    emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(status)));
}

bool MpvController::Impl::handleOpeningCommandReply(const mpv_event &event) {
    auto found =
        std::find_if(pendingOpens.begin(), pendingOpens.end(), [&event](const PendingOpen &open) {
            return open.command == event.reply_userdata;
        });
    if (found == pendingOpens.end()) {
        if (event.reply_userdata != openingCommand || !openingCommand)
            return false;
        openingCommand = 0;
    } else if (event.error < 0) {
        rejectPendingOpen(event.reply_userdata, event.error);
        return true;
    }
    if (event.data) {
        const auto &result = static_cast<mpv_event_command *>(event.data)->result;
        if (result.format == MPV_FORMAT_NODE_MAP) {
            const auto &map = *result.u.list;
            for (int index = 0; index < map.num; ++index) {
                if (QByteArrayView(map.keys[index]) == "playlist_entry_id" &&
                    map.values[index].format == MPV_FORMAT_INT64) {
                    const auto entry = map.values[index].u.int64;
                    if (found != pendingOpens.end()) {
                        found->entry = entry;
                        found->replyReceived = true;
                        if (startedEntries.remove(entry)) {
                            const auto generation = found->generation;
                            commitPendingOpen(entry, true);
                            if (generation == openGeneration) {
                                pendingPlayback.reset();
                                emit owner->stateChanged();
                                if (endedEntries.contains(entry)) {
                                    loadedEntries.remove(entry);
                                    const auto end = endedEntries.take(entry);
                                    handleEndFile(end);
                                } else if (loadedEntries.remove(entry)) {
                                    handleFileLoaded();
                                }
                            } else {
                                loadedEntries.remove(entry);
                                endedEntries.remove(entry);
                            }
                        }
                    } else {
                        openingEntry = entry;
                    }
                }
            }
        }
    }
    if (event.reply_userdata == openingCommand)
        openingCommand = 0;
    return true;
}

void MpvController::setHardwareDecodingEnabled(bool enabled) {
    if (enabled == impl_->hardwareDecodingEnabled)
        return;
    if (impl_->command({"set", "hwdec", enabled ? "auto-safe" : "no"})) {
        // Keep accepted intent before asynchronous observations or an immediate save.
        // Decoder fallback is separate and reported by hwdec-current.
        impl_->hardwareDecodingEnabled = enabled;
        emit hardwareDecodingPreferenceChanged(enabled);
    }
}

void MpvController::openFile(const QString &path) { openFileAtPosition(path, 0); }

void MpvController::setSuppressAutoLoadForNextOpen(bool suppress) {
    impl_->suppressAutoLoadForNextOpen = suppress;
}

void MpvController::openFileAtPosition(const QString &path, double seconds) {
    const bool suppressAutoLoad = std::exchange(impl_->suppressAutoLoadForNextOpen, false);
    if (!isReady() || path.isEmpty() || !std::isfinite(seconds) || seconds < 0)
        return;
    const auto generation = ++impl_->openGeneration;
    impl_->openingPosition = seconds;
    impl_->openingEntry = -1;
    auto options = QByteArray("aid=auto,sid=auto,start=") + QByteArray::number(seconds, 'f', 3);
    if (suppressAutoLoad)
        options += ",sub-auto=no";
    const auto [id, status] =
        impl_->submitCommand({"loadfile", path.toUtf8(), "replace", "-1", options});
    impl_->openingCommand = id;
    if (id) {
        impl_->pendingOpens.append({id, generation, path});
        impl_->state.path = path;
        auto pending = impl_->state;
        pending.active = false;
        pending.seekable = false;
        pending.position = 0;
        pending.duration = 0;
        pending.currentChapter = -1;
        pending.hardwareDecoder.clear();
        pending.path = path;
        impl_->pendingPlayback = std::move(pending);
    }
    impl_->cancelPreferenceRestore();
    if (generation != impl_->openGeneration)
        return;
    emit fileOpenRequested();
    if (id) {
        emit stateChanged();
        if (impl_->information != MediaInformation{})
            emit mediaInformationChanged();
        if (!impl_->tracks.isEmpty())
            emit tracksChanged();
        if (!impl_->chapters.isEmpty())
            emit chaptersChanged();
        if (impl_->abLoop != AbLoop{})
            emit abLoopChanged();
    }
    if (!id)
        impl_->notifySynchronousOpenFailure(generation, status);
    impl_->report(status);
}
void MpvController::setPaused(bool paused) {
    if (state().active)
        impl_->property("pause", MPV_FORMAT_FLAG, int(paused));
}
void MpvController::togglePause() { setPaused(!state().paused); }
void MpvController::stop() {
    const auto requestedGeneration = impl_->openGeneration;
    emit stopRequested();
    if (requestedGeneration != impl_->openGeneration)
        return;
    impl_->mediaReady = false;
    impl_->openingPosition.reset();
    if (!impl_->pendingOpens.isEmpty())
        impl_->state.path = impl_->playingPath;
    const auto generation = ++impl_->openGeneration;
    impl_->pendingPlayback.reset();
    impl_->openingCommand = 0;
    impl_->openingEntry = -1;
    impl_->pendingOpens.clear();
    impl_->startedEntries.clear();
    impl_->loadedEntries.clear();
    impl_->endedEntries.clear();
    impl_->openGenerationsByEntry.clear();
    impl_->cancelInformationQueries();
    const bool informationChanged = impl_->information != MediaInformation{};
    impl_->information = {};
    impl_->recoveringRenderer = false;
    impl_->resetAbLoop();
    impl_->cancelChapterRequest();
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
    if (generation != impl_->openGeneration)
        return;
    impl_->command({"stop"});
    if (informationChanged)
        emit mediaInformationChanged();
}
void MpvController::setFileLoopEnabled(bool enabled) {
    impl_->command({"set", "loop-file", enabled ? "inf" : "no"});
}
void MpvController::seekAbsolute(double seconds) {
    if (!state().seekable || !std::isfinite(seconds))
        return;
    const double target = std::clamp(seconds, 0.0, state().duration);
    impl_->cancelChapterRequest();
    impl_->command({"seek", QByteArray::number(target, 'f', 3), "absolute+exact"});
}
void MpvController::seekRelative(double seconds) {
    if (!state().seekable || !std::isfinite(seconds))
        return;
    impl_->cancelChapterRequest();
    impl_->command({"seek", QByteArray::number(seconds, 'f', 3), "relative+exact"});
}
void MpvController::setVolume(double volume) {
    if (isReady() && std::isfinite(volume)) {
        const auto bounded = std::clamp(volume, 0.0, 100.0);
        impl_->property("volume", MPV_FORMAT_DOUBLE, bounded);
        emit volumeRequested(bounded);
    }
}
void MpvController::setMuted(bool muted) {
    if (isReady()) {
        impl_->property("mute", MPV_FORMAT_FLAG, int(muted));
        emit muteRequested(muted);
    }
}

void MpvController::selectAudioTrack(qint64 id) { impl_->selectTrack(MediaTrack::Type::Audio, id); }

void MpvController::setPlaybackSpeed(double multiplier) {
    impl_->setTuning(impl_->speed, multiplier);
}
void MpvController::adjustPlaybackSpeed(double delta) {
    setPlaybackSpeed(impl_->speed.requested.value_or(state().playbackSpeed) + delta);
}
void MpvController::setAudioDelay(double seconds) { impl_->setTuning(impl_->audioDelay, seconds); }
void MpvController::adjustAudioDelay(double seconds) {
    setAudioDelay(impl_->audioDelay.requested.value_or(state().audioDelay) + seconds);
}
void MpvController::setSubtitleDelay(double seconds) {
    impl_->setTuning(impl_->subtitleDelay, seconds);
}
void MpvController::adjustSubtitleDelay(double seconds) {
    setSubtitleDelay(impl_->subtitleDelay.requested.value_or(state().subtitleDelay) + seconds);
}

void MpvController::selectSubtitleTrack(qint64 id) {
    impl_->selectTrack(MediaTrack::Type::Subtitle, id);
}

void MpvController::prepareForRendererReset() {
    if (state().active) {
        impl_->recovery = Impl::RecoveryState{
            state(), impl_->tracksToRestore.isEmpty() ? tracks() : impl_->tracksToRestore,
            impl_->explicitExternalPaths};
        if (impl_->openingPosition)
            impl_->recovery->playback.position = *impl_->openingPosition;
        impl_->recoveringRenderer = true;
    }
}

void MpvController::resumeAfterRendererReset() {
    if (!impl_->recovery)
        return;
    const auto saved = std::move(*impl_->recovery);
    impl_->recovery.reset();
    const auto generation = impl_->openGeneration;
    const auto identity = impl_->restoreIdentity + 1;
    impl_->cancelTrackRestore();
    if (generation != impl_->openGeneration || identity != impl_->restoreIdentity)
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
    impl_->tracksToRestore = saved.tracks;
    impl_->state.path = saved.playback.path;
    impl_->playingPath = saved.playback.path;
    impl_->openingPosition = position;
    impl_->openingEntry = -1;
    impl_->resetPlayback();
    impl_->explicitExternalPathsToRestore = std::move(saved.explicitExternalPaths);
    emit stateChanged();
    if (generation != impl_->openGeneration || identity != impl_->restoreIdentity)
        return;
    const auto [id, status] =
        impl_->submitCommand({"loadfile", saved.playback.path.toUtf8(), "replace", "-1", options});
    impl_->openingCommand = id;
    if (!id) {
        // No END_FILE will follow a rejected submission. Cancel recovery before
        // reporting errors so a reentrant new media request keeps its state.
        impl_->recoveringRenderer = false;
        impl_->openingPosition.reset();
        impl_->cancelTrackRestore();
        impl_->resetAbLoop();
    }
    impl_->report(status);
}
