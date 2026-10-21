#include "mpvcontroller.h"

#include <QMetaObject>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <tuple>
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
    HardwareDecoder
};
} // namespace

struct MpvController::Impl {
    MpvController *owner;
    std::unique_ptr<mpv_handle, MpvDeleter> client;
    PlaybackState state;
    std::optional<PlaybackState> recovery;
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

    void command(std::initializer_list<QByteArray> arguments) {
        if (!ready)
            return;
        std::vector<const char *> pointers;
        pointers.reserve(arguments.size() + 1);
        for (const auto &argument : arguments)
            pointers.push_back(argument.constData());
        pointers.push_back(nullptr);
        report(mpv_command_async(client.get(), ++request, pointers.data()));
    }

    template <typename T> void property(const char *name, mpv_format format, T value) {
        if (ready)
            report(mpv_set_property_async(client.get(), ++request, name, format, &value));
    }

    void resetPlayback() {
        state.active = false;
        state.seekable = false;
        state.position = 0;
        state.duration = 0;
        state.hardwareDecoder.clear();
    }

    void updateProperty(const mpv_event &event) {
        const auto &value = *static_cast<mpv_event_property *>(event.data);
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
                emit owner->stateChanged();
                break;
            case MPV_EVENT_FILE_LOADED:
                recoveringRenderer = false;
                state.active = true;
                error.clear();
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
                   std::tuple{HardwareDecoder, "hwdec-current", MPV_FORMAT_STRING}};
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

void MpvController::openFile(const QString &path) {
    if (!isReady() || path.isEmpty())
        return;
    impl_->state.path = path;
    impl_->recovery.reset();
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
    if (std::isfinite(volume))
        impl_->property("volume", MPV_FORMAT_DOUBLE, std::clamp(volume, 0.0, 100.0));
}
void MpvController::setMuted(bool muted) { impl_->property("mute", MPV_FORMAT_FLAG, int(muted)); }

void MpvController::prepareForRendererReset() {
    if (state().active) {
        impl_->recovery = state();
        impl_->recoveringRenderer = true;
    }
}

void MpvController::resumeAfterRendererReset() {
    if (!impl_->recovery)
        return;
    const auto saved = *impl_->recovery;
    impl_->recovery.reset();
    // Releasing an mpv renderer disables video. Reload at the saved position,
    // including video-only files whose core may have reached END_FILE.
    const auto options = QByteArray("start=") + QByteArray::number(saved.position, 'f', 3) +
                         ",pause=" + (saved.paused ? "yes" : "no");
    impl_->state.path = saved.path;
    impl_->resetPlayback();
    emit stateChanged();
    impl_->command({"loadfile", saved.path.toUtf8(), "replace", "-1", options});
}
