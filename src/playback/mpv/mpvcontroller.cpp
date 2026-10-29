#include "playback/media/mediasource.h"
#include "playback/mpv/mpvcontroller_p.h"

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
    // The built-in ytdl hook is not disabled by load-scripts=no. Web-page
    // extraction is unverified, so keep external processes out of the player.
    // Builds without the built-in script do not expose the option; that must not
    // disable playback.
    backend.softOption("ytdl", "no");
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

bool MpvController::Impl::handleRecoveryCommandReply(const mpv_event &event) {
    if (!recoveryLoad || !recoveryLoad->command || event.reply_userdata != recoveryLoad->command)
        return false;
    recoveryLoad->command = 0;
    if (openingCommand == event.reply_userdata)
        openingCommand = 0;
    if (event.error < 0) {
        // The reload was rejected before it produced a file, so no END_FILE
        // will follow to report the failure.
        failRecoveryLoad(event.error, openGeneration);
        return true;
    }
    if (event.data) {
        const auto &result = static_cast<mpv_event_command *>(event.data)->result;
        if (result.format == MPV_FORMAT_NODE_MAP) {
            const auto &map = *result.u.list;
            for (int index = 0; index < map.num; ++index) {
                if (QByteArrayView(map.keys[index]) != "playlist_entry_id" ||
                    map.values[index].format != MPV_FORMAT_INT64)
                    continue;
                recoveryLoad->entry = map.values[index].u.int64;
                recoveryLoad->entryKnown = true;
                // Match normal opens: keep the opening entry until a position
                // observation confirms playback started.
                openingEntry = recoveryLoad->entry;
                // FILE_LOADED may precede its command reply. If the reload is
                // already playing, the snapshot can be retired now.
                if (state.active && playingEntry == recoveryLoad->entry)
                    finishRecoveryLoad();
                break;
            }
        }
    }
    // A failure can queue its END_FILE before this reply correlated the entry
    // (START_FILE is then parked in startedEntries). Replay it now that the
    // entry is known so the failure is not silently dropped.
    if (recoveryLoad && recoveryLoad->entryKnown && endedEntries.contains(recoveryLoad->entry)) {
        const auto end = endedEntries.take(recoveryLoad->entry);
        handleEndFile(end);
    }
    return true;
}

bool MpvController::Impl::handleOpeningCommandReply(const mpv_event &event) {
    if (handleRecoveryCommandReply(event))
        return true;
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
    // An explicit playback request supersedes a retained recovery snapshot and
    // any reload still in flight for it. Tombstone the reload's entry like
    // stop() does so its queued terminal event cannot report a stale failure.
    if (impl_->recoveryLoad) {
        if (impl_->recoveryLoad->entryKnown)
            impl_->invalidateEntry(impl_->recoveryLoad->entry);
        if (impl_->recoveryLoad->command)
            impl_->canceledCommands.insert(impl_->recoveryLoad->command);
        impl_->recoveryLoad.reset();
    }
    impl_->recovery.reset();
    impl_->recoveryAttempts = 0;
    impl_->recoveringRenderer = false;
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
    // A stop invalidates outstanding commands. Keep their ids until mpv delivers
    // the replies so a canceled command cannot surface as a user-visible error.
    const auto cancelReply = [this](uint64_t command) {
        if (command)
            impl_->canceledCommands.insert(command);
    };
    cancelReply(impl_->openingCommand);
    for (const auto &open : std::as_const(impl_->pendingOpens))
        cancelReply(open.command);
    cancelReply(impl_->chapterCommand);
    // Keep screenshotCommand pending: its own reply still ends the operation, but
    // a failed write after stop must not be reported as a playback error.
    cancelReply(impl_->screenshotCommand);
    const auto generation = ++impl_->openGeneration;
    impl_->pendingPlayback.reset();
    impl_->openingCommand = 0;
    impl_->openingEntry = -1;
    // Tombstone every entry the stop invalidates before clearing the maps.
    // A queued END_FILE/FILE_LOADED for these entries must be ignored instead of
    // falling back to the current generation.
    impl_->invalidateEntry(impl_->playingEntry);
    for (const auto entry : impl_->openGenerationsByEntry.keys())
        impl_->invalidateEntry(entry);
    for (const auto entry : std::as_const(impl_->startedEntries))
        impl_->invalidateEntry(entry);
    for (const auto entry : std::as_const(impl_->loadedEntries))
        impl_->invalidateEntry(entry);
    for (const auto entry : impl_->endedEntries.keys())
        impl_->invalidateEntry(entry);
    impl_->playingEntry = -1;
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
    impl_->recoveryLoad.reset();
    impl_->recoveryAttempts = 0;
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
    // mpv can seek before it knows the duration (for example while a live stream
    // resolves). Only clamp above when the duration is a known positive value.
    const double duration = state().duration;
    const double target = std::isfinite(duration) && duration > 0.0
                              ? std::clamp(seconds, 0.0, duration)
                              : std::max(seconds, 0.0);
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
namespace {
constexpr std::array equalizerBands{31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
constexpr std::array<std::array<int, 10>, 7> equalizerGains{{
    {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {{5, 4, 3, 1, -1, -1, 0, 2, 3, 4}},
    {{-1, 0, 2, 4, 4, 2, 0, -1, -1, -1}},
    {{4, 3, 1, 2, -1, -1, 0, 1, 2, 3}},
    {{4, 3, 2, 1, 0, 0, 1, 2, 3, 4}},
    {{-3, -2, -1, 1, 3, 4, 4, 2, 0, -1}},
    {{6, 5, 4, 2, 1, 0, 0, 0, 0, 0}},
}};
QByteArray equalizerChain(EqualizerPreset preset) {
    if (preset == EqualizerPreset::Off)
        return {}; // mpv rejects "no" here; an empty value clears the chain.
    const auto &gains = equalizerGains[static_cast<size_t>(preset) - 1];
    QByteArray chain;
    for (size_t index = 0; index < gains.size(); ++index)
        chain += "equalizer=f=" + QByteArray::number(equalizerBands[index]) +
                 ":width_type=o:width=1:g=" + QByteArray::number(gains[index]) + ',';
    chain.chop(1);
    return chain;
}
QByteArray videoChain(SharpenLevel sharpen, DenoiseLevel denoise) {
    static constexpr std::array sharpenAmounts{0.0, 0.5, 1.0, 1.5};
    static constexpr std::array denoiseStrength{"", "2:1.5:3:2.25", "4:3:6:4.5", "6:4.5:9:6.75"};
    QStringList filters;
    if (sharpen != SharpenLevel::Off)
        filters << QStringLiteral("unsharp=luma_msize_x=5:luma_msize_y=5:luma_amount=%1")
                       .arg(sharpenAmounts[static_cast<size_t>(sharpen)], 0, 'f', 1);
    if (denoise != DenoiseLevel::Off)
        filters << QStringLiteral("hqdn3d=") +
                       QLatin1String(denoiseStrength[static_cast<size_t>(denoise)]);
    return filters.isEmpty() ? QByteArray() : filters.join(',').toUtf8();
}
} // namespace
EqualizerPreset MpvController::equalizerPreset() const {
    return static_cast<EqualizerPreset>(property("equalizerPreset").toInt());
}
SharpenLevel MpvController::sharpenLevel() const {
    return static_cast<SharpenLevel>(property("sharpenLevel").toInt());
}
DenoiseLevel MpvController::denoiseLevel() const {
    return static_cast<DenoiseLevel>(property("denoiseLevel").toInt());
}
void MpvController::setEqualizerPreset(EqualizerPreset preset) {
    if (preset == equalizerPreset())
        return;
    // af is the only audio filter chain today, so a preset replaces it wholesale.
    if (!impl_->command({"set", "af", equalizerChain(preset)}))
        return;
    setProperty("equalizerPreset", static_cast<int>(preset));
    emit equalizerPresetChanged();
}
void MpvController::setVideoFilters(SharpenLevel sharpen, DenoiseLevel denoise) {
    if (sharpen == sharpenLevel() && denoise == denoiseLevel())
        return;
    // vf is a single chain (nothing else sets it today); deinterlace is separate.
    if (!impl_->command({"set", "vf", videoChain(sharpen, denoise)}))
        return;
    setProperty("sharpenLevel", static_cast<int>(sharpen));
    setProperty("denoiseLevel", static_cast<int>(denoise));
    emit videoFiltersChanged();
}
