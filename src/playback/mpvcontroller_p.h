#pragma once

#include "mpvchapters.h"
#include "mpvcontroller.h"
#include "mpvtracks.h"

#include <QFileInfo>
#include <QHash>
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
    Chapter,
    VideoImage,
    FileFormat,
    FileSize,
    VideoCodec,
    VideoWidth,
    VideoHeight,
    VideoPixelFormat,
    VideoPrimaries,
    VideoTransfer,
    ContainerFrameRate,
    AudioCodec,
    AudioSampleRate,
    AudioChannels,
    AudioSampleFormat,
    AudioDevices,
    SelectedAudioDevice
};
inline constexpr auto informationProperties =
    std::array{std::tuple{FileFormat, "file-format", MPV_FORMAT_STRING},
               std::tuple{FileSize, "file-size", MPV_FORMAT_INT64},
               std::tuple{VideoCodec, "current-tracks/video/codec", MPV_FORMAT_STRING},
               std::tuple{VideoWidth, "video-params/w", MPV_FORMAT_INT64},
               std::tuple{VideoHeight, "video-params/h", MPV_FORMAT_INT64},
               std::tuple{VideoPixelFormat, "video-params/pixelformat", MPV_FORMAT_STRING},
               std::tuple{VideoPrimaries, "video-params/primaries", MPV_FORMAT_STRING},
               std::tuple{VideoTransfer, "video-params/gamma", MPV_FORMAT_STRING},
               std::tuple{ContainerFrameRate, "container-fps", MPV_FORMAT_DOUBLE},
               std::tuple{AudioCodec, "current-tracks/audio/codec", MPV_FORMAT_STRING},
               std::tuple{AudioSampleRate, "audio-params/samplerate", MPV_FORMAT_INT64},
               std::tuple{AudioChannels, "audio-params/channels", MPV_FORMAT_STRING},
               std::tuple{AudioSampleFormat, "audio-params/format", MPV_FORMAT_STRING}};
} // namespace MpvPrivate

struct MpvController::Impl {
    using enum MpvPrivate::Property;
    struct RecoveryState {
        PlaybackState playback;
        QList<MediaTrack> tracks;
        QSet<QString> explicitExternalPaths;
    };
    struct PendingOpen {
        uint64_t command = 0;
        uint64_t generation = 0;
        QString path;
        int64_t entry = -1;
        bool replyReceived = false;
    };
    struct TrackPreferenceRestore {
        TrackSelectionPreference preference;
        QList<std::pair<MediaTrack::Type, QString>> paths;
        uint64_t generation = 0;
        uint64_t command = 0;
        uint64_t query = 0;
        uint64_t identity = 0;
        bool selecting = false;
        QList<std::pair<MediaTrack::Type, qint64>> selections;
    };
    MpvController *owner;
    std::unique_ptr<mpv_handle, MpvPrivate::MpvDeleter> client;
    PlaybackState state;
    std::optional<PlaybackState> pendingPlayback;
    MediaInformation information;
    // Zero marks a canceled query; retain its id until its reply is consumed.
    QHash<uint64_t, uint64_t> informationQueries;
    bool informationRefreshQueued = false;
    QList<MediaTrack> tracks;
    QList<MediaChapter> chapters;
    std::optional<int> requestedChapter;
    uint64_t chapterCommand = 0;
    uint64_t chapterQuery = 0;
    bool chapterSeekStarted = false;
    bool chapterQuerySettled = false;
    bool loading = false;
    std::optional<RecoveryState> recovery;
    // Canonical paths the user explicitly added or selected for this media.
    QSet<QString> explicitExternalPaths;
    // Explicit paths captured before a renderer-recovery reset; re-marked once
    // restoreExternalTracks has reloaded them.
    QSet<QString> explicitExternalPathsToRestore;
    QList<MediaTrack> tracksToRestore;
    QList<std::pair<MediaTrack::Type, QString>> externalPathsToRestore;
    uint64_t restoreCommand = 0;
    uint64_t restoreQuery = 0;
    uint64_t restoreGeneration = 0;
    uint64_t restoreIdentity = 0;
    QSet<uint64_t> canceledRestoreRequests;
    bool applyingRestoredSelections = false;
    std::optional<TrackPreferenceRestore> preferenceRestore;
    bool recoveringRenderer = false;
    QString error;
    std::atomic_bool shuttingDown{false};
    bool ready = false;
    bool hardwareDecodingEnabled = true;
    SubtitlePreferences subtitles;
    VideoPresentation presentation;
    VideoFraming framing;
    VideoColorAdjustment videoColor;
    HdrToneMapping hdrToneMapping = HdrToneMapping::Auto;
    DeinterlaceMode deinterlaceMode = DeinterlaceMode::Auto;
    QList<AudioDevice> audioDevices;
    QString selectedAudioDevice = QStringLiteral("auto");
    uint64_t audioDeviceRequest = 0;
    QString requestedAudioDevice;
    bool persistAudioDeviceRequest = false;
    AbLoop abLoop;
    bool mediaReady = false;
    QList<PendingOpen> pendingOpens;
    QSet<int64_t> startedEntries;
    QSet<int64_t> loadedEntries;
    QHash<int64_t, mpv_event_end_file> endedEntries;
    QHash<int64_t, uint64_t> openGenerationsByEntry;
    QString playingPath;
    uint64_t openGeneration = 0;
    // Preserve load intent until the matching native file reports its position.
    std::optional<double> openingPosition;
    uint64_t openingCommand = 0;
    int64_t openingEntry = -1;
    int64_t playingEntry = -1;
    bool hasMotionVideo = false;
    uint64_t screenshotCommand = 0;
    QString screenshotPath;
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

    void setFramingComponent(const char *name, double value, double VideoFraming::*member,
                             double minimum, double maximum);

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

    std::pair<uint64_t, int> submitCommand(std::initializer_list<QByteArray> arguments) {
        if (!ready)
            return {0, MPV_ERROR_UNINITIALIZED};
        std::vector<const char *> pointers;
        pointers.reserve(arguments.size() + 1);
        for (const auto &argument : arguments)
            pointers.push_back(argument.constData());
        pointers.push_back(nullptr);
        const auto id = ++request;
        const int status = mpv_command_async(client.get(), id, pointers.data());
        return {status >= 0 ? id : 0, status};
    }

    uint64_t command(std::initializer_list<QByteArray> arguments) {
        if (!ready)
            return 0;
        const auto [id, status] = submitCommand(arguments);
        report(status);
        return id;
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
            emit owner->tuningRequested(speed.requested.value_or(state.playbackSpeed),
                                        audioDelay.requested.value_or(state.audioDelay),
                                        subtitleDelay.requested.value_or(state.subtitleDelay));
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
        mediaReady = false;
        explicitExternalPaths.clear();
        cancelInformationQueries();
        const bool informationChanged = information != MediaInformation{};
        information = {};
        resetAbLoop();
        loading = false;
        state.active = false;
        state.seekable = false;
        hasMotionVideo = false;
        state.position = 0;
        state.duration = 0;
        state.hardwareDecoder.clear();
        state.currentChapter = -1;
        cancelChapterRequest();
        updateChapters({});
        updateTracks({});
        if (informationChanged)
            emit owner->mediaInformationChanged();
    }

    void updateChapters(QList<MediaChapter> snapshot) {
        if (chapters == snapshot)
            return;
        chapters = std::move(snapshot);
        emit owner->chaptersChanged();
    }

    void cancelChapterRequest();
    void resetAbLoop();
    void observeChapter(int64_t index, bool settled = false);
    void selectChapter(int index);
    void queryChapterRequest(bool settled = false);
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
        const auto selected =
            std::find_if(tracks.cbegin(), tracks.cend(), [type, id](const MediaTrack &track) {
                return track.type == type && track.id == id;
            });
        if (id != 0 && selected == tracks.cend()) {
            error = MpvController::tr("所选轨道不存在");
            emit owner->errorOccurred(error);
            return;
        }
        QString externalPath;
        if (id != 0 && selected->external)
            externalPath = selected->externalPath;
        const auto generation = openGeneration;
        cancelTrackRestore();
        if (generation != openGeneration || pendingPlayback || !state.active)
            return;
        // Selecting an auto-loaded external track is user intent; record it
        // before signaling so a synchronous persistence snapshot sees it.
        if (!externalPath.isEmpty())
            explicitExternalPaths.insert(canonicalExternalPath(externalPath));
        emit owner->trackSelectionRequested(type, id);
        if (generation != openGeneration || pendingPlayback || !state.active)
            return;
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
        explicitExternalPathsToRestore.clear();
        externalPathsToRestore.clear();
        ++restoreIdentity;
        restoreCommand = restoreQuery = 0;
        cancelPreferenceRestore();
    }

    static QString canonicalExternalPath(const QString &path);
    void addExternalTrack(MediaTrack::Type type, const QString &path);
    void cancelPreferenceRestore();
    void startPreferenceExternalLoad();
    void queryPreferenceTracks();
    void applyPreferenceTracks(const QList<MediaTrack> &snapshot);
    void startPreferenceSelection();
    bool finishPreferenceCommand(const mpv_event &event);
    bool finishPreferenceQuery(const mpv_event &event);

    void queryRestoredTracks();
    void restoreExternalTracks();
    void restoreNextExternalTrack();
    void restoreSelections(const QList<MediaTrack> &current);

    void updateProperty(const mpv_event &event);
    void requestAudioDevice(const QString &name, bool persistPreference);
    void confirmAudioDevice(const mpv_event &event);
    bool updateMediaInformation(const mpv_event &event);
    void refreshMediaInformation();
    void scheduleInformationRefresh();
    void cancelInformationQueries();
    bool finishInformationQuery(const mpv_event &event);
    bool finishScreenshot(const mpv_event &event);
    void drainEvents();
    void handleEndFile(const mpv_event_end_file &end);
    void handleFileLoaded();

    static void wakeup(void *userdata) {
        auto *self = static_cast<Impl *>(userdata);
        if (!self->shuttingDown.load()) {
            QMetaObject::invokeMethod(
                self->owner, [self] { self->drainEvents(); }, Qt::QueuedConnection);
        }
    }

    void commitPendingOpen(int64_t entry, bool alreadyStarted = false);
    void rejectPendingOpen(uint64_t command, int status);
    void notifySynchronousOpenFailure(uint64_t generation, int status);
    bool handleOpeningCommandReply(const mpv_event &event);
};
