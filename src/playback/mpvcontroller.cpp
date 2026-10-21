#include "mpvcontroller.h"
#include "mpvtracks.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace {
struct MpvDeleter {
    void operator()(mpv_handle *handle) const { mpv_terminate_destroy(handle); }
};
enum Property : uint64_t {
    Position = 1,
    Duration,
    Paused,
    Volume,
    Muted,
    Seekable,
    HardwareDecoder,
    Tracks
};
} // namespace

struct MpvController::Impl {
    struct RecoveryState {
        PlaybackState playback;
        QList<MediaTrack> tracks;
    };
    MpvController *owner;
    std::unique_ptr<mpv_handle, MpvDeleter> client;
    PlaybackState state;
    QList<MediaTrack> tracks;
    bool loading = false;
    std::optional<RecoveryState> recovery;
    QList<MediaTrack> tracksToRestore;
    QStringList subtitlePathsToRestore;
    uint64_t restoreCommand = 0;
    uint64_t restoreQuery = 0;
    QSet<uint64_t> canceledRestoreRequests;
    bool applyingRestoredSelections = false;
    bool recoveringRenderer = false;
    QString error;
    std::atomic_bool shuttingDown{false};
    bool ready = false;
    uint64_t request = 0;

    explicit Impl(MpvController *controller) : owner(controller), client(mpv_create()) {}

    void report(int status) {
        if (status < 0) {
            error = QString::fromUtf8(mpv_error_string(status));
            emit owner->errorOccurred(error);
        }
    }

    bool option(const char *name, const char *value) {
        const int status = mpv_set_option_string(client.get(), name, value);
        report(status);
        return status >= 0;
    }

    uint64_t command(std::initializer_list<QByteArray> arguments) {
        if (!ready)
            return 0;
        std::vector<const char *> pointers;
        pointers.reserve(arguments.size() + 1);
        for (const auto &argument : arguments)
            pointers.push_back(argument.constData());
        pointers.push_back(nullptr);
        const auto id = ++request;
        const int status = mpv_command_async(client.get(), id, pointers.data());
        report(status);
        return status >= 0 ? id : 0;
    }

    template <typename T> void property(const char *name, mpv_format format, T value) {
        if (ready)
            report(mpv_set_property_async(client.get(), ++request, name, format, &value));
    }

    void resetPlayback() {
        loading = false;
        state.active = false;
        state.seekable = false;
        state.position = 0;
        state.duration = 0;
        state.hardwareDecoder.clear();
        updateTracks({});
    }

    void updateTracks(QList<MediaTrack> snapshot) {
        if (applyingRestoredSelections && trackSelectionRestored(tracksToRestore, snapshot))
            cancelTrackRestore();
        if (tracks == snapshot)
            return;
        tracks = std::move(snapshot);
        emit owner->tracksChanged();
    }

    void selectTrack(MediaTrack::Type type, qint64 id) {
        if (!state.active)
            return;
        const bool valid = id == 0 || std::any_of(tracks.cbegin(), tracks.cend(),
                                                  [type, id](const MediaTrack &track) {
                                                      return track.type == type && track.id == id;
                                                  });
        if (!valid) {
            error = MpvController::tr("所选轨道不存在");
            emit owner->errorOccurred(error);
            return;
        }
        cancelTrackRestore();
        command({"set", type == MediaTrack::Type::Audio ? "aid" : "sid",
                 id == 0 ? QByteArray("no") : QByteArray::number(id)});
    }

    void cancelTrackRestore() {
        if (restoreCommand)
            canceledRestoreRequests.insert(restoreCommand);
        if (restoreQuery)
            canceledRestoreRequests.insert(restoreQuery);
        applyingRestoredSelections = false;
        tracksToRestore.clear();
        subtitlePathsToRestore.clear();
        restoreCommand = restoreQuery = 0;
    }

    void queryRestoredTracks() {
        restoreQuery = ++request;
        const int status =
            mpv_get_property_async(client.get(), restoreQuery, "track-list", MPV_FORMAT_NODE);
        report(status);
        if (status < 0) {
            restoreQuery = 0;
            cancelTrackRestore();
        }
    }

    void restoreExternalTracks() {
        if (tracksToRestore.isEmpty())
            return;
        QSet<QString> paths;
        for (const auto &track : tracksToRestore) {
            if (track.type == MediaTrack::Type::Subtitle && track.external &&
                !track.externalPath.isEmpty() && !paths.contains(track.externalPath)) {
                paths.insert(track.externalPath);
                subtitlePathsToRestore.append(track.externalPath);
            }
        }
        restoreNextSubtitle();
    }

    void restoreNextSubtitle() {
        // sub-add spawns backend workers. Chain replies so the final query sees all files.
        while (!subtitlePathsToRestore.isEmpty()) {
            const auto path = subtitlePathsToRestore.takeFirst();
            restoreCommand = command({"sub-add", path.toUtf8(), "auto"});
            if (restoreCommand)
                return;
        }
        queryRestoredTracks();
    }

    void restoreSelections(const QList<MediaTrack> &current) {
        for (const auto type : {MediaTrack::Type::Audio, MediaTrack::Type::Subtitle}) {
            const auto id = trackIdForRestore(tracksToRestore, current, type);
            command({"set", type == MediaTrack::Type::Audio ? "aid" : "sid",
                     id ? QByteArray::number(id) : QByteArray("no")});
        }
        applyingRestoredSelections = true;
        updateTracks(current);
    }

    void updateProperty(const mpv_event &event) {
        const auto &value = *static_cast<mpv_event_property *>(event.data);
        if (event.reply_userdata == Tracks) {
            updateTracks(value.data && (state.active || loading)
                             ? parseMpvTracks(*static_cast<mpv_node *>(value.data))
                             : QList<MediaTrack>{});
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
            default:
                return;
            }
            emit owner->stateChanged();
            return;
        }
        switch (event.reply_userdata) {
        case Position:
            if (state.active)
                state.position = *static_cast<double *>(value.data);
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

    void drainEvents() {
        if (shuttingDown)
            return;
        while (const auto *event = mpv_wait_event(client.get(), 0)) {
            switch (event->event_id) {
            case MPV_EVENT_NONE:
                return;
            case MPV_EVENT_START_FILE:
                resetPlayback();
                loading = true;
                emit owner->stateChanged();
                break;
            case MPV_EVENT_FILE_LOADED:
                loading = false;
                recoveringRenderer = false;
                state.active = true;
                error.clear();
                restoreExternalTracks();
                emit owner->stateChanged();
                emit owner->fileLoaded();
                break;
            case MPV_EVENT_END_FILE: {
                const auto &end = *static_cast<mpv_event_end_file *>(event->data);
                resetPlayback();
                emit owner->stateChanged();
                const bool expectedReset =
                    recoveringRenderer && end.error == MPV_ERROR_VO_INIT_FAILED;
                if (end.reason == MPV_END_FILE_REASON_ERROR && !expectedReset) {
                    emit owner->mediaLoadFailed(QString::fromUtf8(mpv_error_string(end.error)));
                    report(end.error);
                }
                if (end.reason == MPV_END_FILE_REASON_EOF)
                    emit owner->playbackFinished();
                break;
            }
            case MPV_EVENT_PROPERTY_CHANGE:
                updateProperty(*event);
                break;
            case MPV_EVENT_COMMAND_REPLY:
                if (canceledRestoreRequests.remove(event->reply_userdata))
                    break;
                if (event->reply_userdata == restoreCommand && restoreCommand) {
                    restoreCommand = 0;
                    restoreNextSubtitle();
                }
                report(event->error);
                break;
            case MPV_EVENT_GET_PROPERTY_REPLY:
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
                report(event->error);
                break;
            case MPV_EVENT_SHUTDOWN:
                ready = false;
                resetPlayback();
                emit owner->stateChanged();
                return;
            default:
                break;
            }
        }
    }

    static void wakeup(void *userdata) {
        auto *self = static_cast<Impl *>(userdata);
        if (!self->shuttingDown.load()) {
            QMetaObject::invokeMethod(
                self->owner, [self] { self->drainEvents(); }, Qt::QueuedConnection);
        }
    }
};

MpvController::MpvController(bool headless, bool silentAudio, QObject *parent)
    : QObject(parent), impl_(std::make_unique<Impl>(this)) {
    auto &backend = *impl_;
    if (!backend.client) {
        backend.error = QStringLiteral("无法创建 libmpv 实例");
        return;
    }
    constexpr auto options =
        std::array{std::pair{"config", "no"}, std::pair{"load-scripts", "no"},
                   std::pair{"terminal", "no"}, std::pair{"osc", "no"},
                   std::pair{"input-default-bindings", "no"}, std::pair{"input-vo-keyboard", "no"},
                   std::pair{"hwdec", "auto-safe"}, std::pair{"idle", "yes"},
                   std::pair{"keep-open", "no"}, std::pair{"video-timing-offset", "0"},
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
        std::array{std::tuple{Position, "time-pos", MPV_FORMAT_DOUBLE},
                   std::tuple{Duration, "duration", MPV_FORMAT_DOUBLE},
                   std::tuple{Paused, "pause", MPV_FORMAT_FLAG},
                   std::tuple{Volume, "volume", MPV_FORMAT_DOUBLE},
                   std::tuple{Muted, "mute", MPV_FORMAT_FLAG},
                   std::tuple{Seekable, "seekable", MPV_FORMAT_FLAG},
                   std::tuple{HardwareDecoder, "hwdec-current", MPV_FORMAT_STRING},
                   std::tuple{Tracks, "track-list", MPV_FORMAT_NODE}};
    for (const auto &[id, name, format] : observations) {
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
const QList<MediaTrack> &MpvController::tracks() const { return impl_->tracks; }

void MpvController::openFile(const QString &path) {
    if (!isReady() || path.isEmpty())
        return;
    impl_->state.path = path;
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
    impl_->recoveringRenderer = false;
    impl_->property("pause", MPV_FORMAT_FLAG, 0);
    impl_->command({"loadfile", path.toUtf8(), "replace"});
}
void MpvController::setPaused(bool paused) {
    if (state().active)
        impl_->property("pause", MPV_FORMAT_FLAG, int(paused));
}
void MpvController::togglePause() { setPaused(!state().paused); }
void MpvController::stop() {
    impl_->recovery.reset();
    impl_->cancelTrackRestore();
    impl_->command({"stop"});
}
void MpvController::seekAbsolute(double seconds) {
    if (!state().seekable || !std::isfinite(seconds))
        return;
    const double target = std::clamp(seconds, 0.0, state().duration);
    impl_->command({"seek", QByteArray::number(target, 'f', 3), "absolute+exact"});
}
void MpvController::seekRelative(double seconds) {
    if (!state().seekable || !std::isfinite(seconds))
        return;
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
    const auto options = QByteArray("start=") +
                         QByteArray::number(saved.playback.position, 'f', 3) +
                         ",pause=" + (saved.playback.paused ? "yes" : "no") +
                         ",aid=" + selectedInternal(MediaTrack::Type::Audio) +
                         ",sid=" + selectedInternal(MediaTrack::Type::Subtitle) + ",sub-auto=no";
    impl_->tracksToRestore = saved.tracks;
    impl_->state.path = saved.playback.path;
    impl_->resetPlayback();
    emit stateChanged();
    impl_->command({"loadfile", saved.playback.path.toUtf8(), "replace", "-1", options});
}
