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
        std::pair{"reset-on-next-file", "ab-loop-a,ab-loop-b"}, std::pair{"keep-open", "no"},
        std::pair{"video-timing-offset", "0"},
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
const PlaybackState &MpvController::state() const { return impl_->state; }
const MediaInformation &MpvController::mediaInformation() const { return impl_->information; }
const QList<MediaTrack> &MpvController::tracks() const { return impl_->tracks; }
const QList<MediaChapter> &MpvController::chapters() const { return impl_->chapters; }
bool MpvController::hardwareDecodingEnabled() const { return impl_->hardwareDecodingEnabled; }

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

void MpvController::openFile(const QString &path) {
    if (!isReady() || path.isEmpty())
        return;
    impl_->mediaReady = false;
    impl_->cancelInformationQueries();
    const bool informationChanged = impl_->information != MediaInformation{};
    impl_->information = {};
    impl_->state.path = path;
    impl_->cancelChapterRequest();
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
    impl_->recoveringRenderer = false;
    impl_->resetAbLoop();
    impl_->property("pause", MPV_FORMAT_FLAG, 0);
    impl_->command({"loadfile", path.toUtf8(), "replace"});
    if (informationChanged)
        emit mediaInformationChanged();
}
void MpvController::setPaused(bool paused) {
    if (state().active)
        impl_->property("pause", MPV_FORMAT_FLAG, int(paused));
}
void MpvController::togglePause() { setPaused(!state().paused); }
void MpvController::stop() {
    impl_->mediaReady = false;
    impl_->cancelInformationQueries();
    const bool informationChanged = impl_->information != MediaInformation{};
    impl_->information = {};
    impl_->recoveringRenderer = false;
    impl_->resetAbLoop();
    impl_->cancelChapterRequest();
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
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

void MpvController::addSubtitle(const QString &path) {
    if (state().active && !path.isEmpty()) {
        impl_->cancelTrackRestore();
        impl_->command({"sub-add", QFileInfo(path).absoluteFilePath().toUtf8(), "select"});
    }
}

void MpvController::prepareForRendererReset() {
    if (state().active) {
        impl_->recovery = Impl::RecoveryState{
            state(), impl_->tracksToRestore.isEmpty() ? tracks() : impl_->tracksToRestore};
        impl_->recoveringRenderer = true;
    }
}

void MpvController::resumeAfterRendererReset() {
    if (!impl_->recovery)
        return;
    const auto saved = std::move(*impl_->recovery);
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
    const auto selectedInternal = [&saved](MediaTrack::Type type) {
        for (const auto &track : saved.tracks) {
            if (track.type == type && track.selected && !track.external)
                return QByteArray::number(track.id);
        }
        return QByteArray("no");
    };
    // Releasing an mpv renderer disables video. Reload at the saved position,
    // including video-only files whose core may have reached END_FILE.
    // Restore the saved external set explicitly to avoid auto-loading duplicates.
    const auto loopPoint = [](const std::optional<double> &value) {
        return value ? QByteArray::number(*value, 'g', 17) : QByteArray("no");
    };
    const auto options =
        QByteArray("start=") + QByteArray::number(saved.playback.position, 'f', 3) +
        ",pause=" + (saved.playback.paused ? "yes" : "no") +
        ",aid=" + selectedInternal(MediaTrack::Type::Audio) +
        ",sid=" + selectedInternal(MediaTrack::Type::Subtitle) + ",sub-auto=no" +
        ",ab-loop-a=" + loopPoint(abLoop().start) + ",ab-loop-b=" + loopPoint(abLoop().end);
    impl_->tracksToRestore = saved.tracks;
    impl_->state.path = saved.playback.path;
    impl_->resetPlayback();
    emit stateChanged();
    const auto [id, status] =
        impl_->submitCommand({"loadfile", saved.playback.path.toUtf8(), "replace", "-1", options});
    if (!id) {
        // No END_FILE will follow a rejected submission. Cancel recovery before
        // reporting errors so a reentrant new media request keeps its state.
        impl_->recoveringRenderer = false;
        impl_->cancelTrackRestore();
        impl_->resetAbLoop();
    }
    impl_->report(status);
}
