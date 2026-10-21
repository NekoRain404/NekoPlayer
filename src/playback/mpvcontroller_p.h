#pragma once

#include "mpvchapters.h"
#include "mpvcontroller.h"
#include "mpvtracks.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <algorithm>
#include <array>
#include <atomic>
#include <climits>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace MpvPrivate {
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
    Tracks,
    Speed,
    AudioDelay,
    SubtitleDelay,
    Chapters,
    Chapter
};
} // namespace MpvPrivate

struct MpvController::Impl {
    using enum MpvPrivate::Property;
    struct RecoveryState {
        PlaybackState playback;
        QList<MediaTrack> tracks;
    };
    MpvController *owner;
    std::unique_ptr<mpv_handle, MpvPrivate::MpvDeleter> client;
    PlaybackState state;
    QList<MediaTrack> tracks;
    QList<MediaChapter> chapters;
    std::optional<int> requestedChapter;
    uint64_t chapterCommand = 0;
    uint64_t chapterQuery = 0;
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
    struct TuningSetting {
        const char *name;
        double minimum;
        double maximum;
        double PlaybackState::*current;
        std::optional<double> requested;
        uint64_t requestId = 0;
        uint64_t queryId = 0;
    };
    TuningSetting speed{"speed", 0.25, 4.0, &PlaybackState::playbackSpeed, {}};
    TuningSetting audioDelay{"audio-delay", -10.0, 10.0, &PlaybackState::audioDelay, {}};
    TuningSetting subtitleDelay{"sub-delay", -600.0, 600.0, &PlaybackState::subtitleDelay, {}};

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

    void setTuning(TuningSetting &setting, double value) {
        if (!ready || !std::isfinite(value))
            return;
        value = std::clamp(value, setting.minimum, setting.maximum);
        const auto id = ++request;
        const auto status =
            mpv_set_property_async(client.get(), id, setting.name, MPV_FORMAT_DOUBLE, &value);
        if (status >= 0) {
            setting.requested = value;
            setting.requestId = id;
            setting.queryId = 0;
        }
        report(status);
    }

    void observeTuning(TuningSetting &setting, double value) {
        state.*setting.current = value;
        emit owner->tuningChanged();
    }

    void confirmTuning(const mpv_event &event) {
        for (auto *setting : {&speed, &audioDelay, &subtitleDelay}) {
            if (event.reply_userdata != setting->requestId)
                continue;
            setting->requestId = 0;
            if (event.error < 0) {
                setting->requested.reset();
                return;
            }
            // Observers omit unchanged values and may coalesce rapid steps.
            // Query only after the latest setter completes; never block the UI.
            setting->queryId = ++request;
            const auto status = mpv_get_property_async(client.get(), setting->queryId,
                                                       setting->name, MPV_FORMAT_DOUBLE);
            if (status < 0) {
                setting->queryId = 0;
                setting->requested.reset();
            }
            report(status);
            return;
        }
    }

    bool finishTuningQuery(const mpv_event &event) {
        for (auto *setting : {&speed, &audioDelay, &subtitleDelay}) {
            if (event.reply_userdata != setting->queryId || !setting->queryId)
                continue;
            setting->queryId = 0;
            setting->requested.reset();
            const auto &value = *static_cast<mpv_event_property *>(event.data);
            if (event.error >= 0 && value.format == MPV_FORMAT_DOUBLE && value.data)
                observeTuning(*setting, *static_cast<double *>(value.data));
            report(event.error);
            return true;
        }
        return false;
    }

    void resetPlayback() {
        loading = false;
        state.active = false;
        state.seekable = false;
        state.position = 0;
        state.duration = 0;
        state.hardwareDecoder.clear();
        state.currentChapter = -1;
        cancelChapterRequest();
        updateChapters({});
        updateTracks({});
    }

    void updateChapters(QList<MediaChapter> snapshot) {
        if (chapters == snapshot)
            return;
        chapters = std::move(snapshot);
        emit owner->chaptersChanged();
    }

    void cancelChapterRequest();
    void observeChapter(int64_t index) {
        state.currentChapter = (state.active || loading) && index >= 0 && index < INT_MAX
                                   ? static_cast<int>(index)
                                   : -1;
    }
    void selectChapter(int index);
    void confirmChapterRequest(const mpv_event &event);
    bool finishChapterQuery(const mpv_event &event);

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
                confirmTuning(*event);
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
