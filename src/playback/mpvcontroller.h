#pragma once

#include "abloop.h"
#include "audiodevice.h"
#include "mediachapter.h"
#include "mediainformation.h"
#include "mediatrack.h"
#include "subtitlepreferences.h"
#include "trackselectionpreference.h"
#include "videocoloradjustment.h"
#include "videodeinterlace.h"
#include "videoframing.h"
#include "videohdr.h"
#include "videopresentation.h"
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <mpv/client.h>

struct PlaybackState {
    bool active = false;
    bool paused = false;
    bool seekable = false;
    bool muted = false;
    double position = 0;
    double duration = 0;
    double volume = 100;
    double playbackSpeed = 1;
    double audioDelay = 0;
    double subtitleDelay = 0;
    int currentChapter = -1;
    QString path;
    QString hardwareDecoder;
};

class MpvController final : public QObject {
    Q_OBJECT
  public:
    explicit MpvController(bool headless = false, bool silentAudio = false,
                           QObject *parent = nullptr);
    ~MpvController() override;
    [[nodiscard]] bool isReady() const;
    [[nodiscard]] QString errorString() const;
    [[nodiscard]] mpv_handle *handle() const;
    [[nodiscard]] const PlaybackState &state() const;
    [[nodiscard]] double resumePosition() const;
    [[nodiscard]] const MediaInformation &mediaInformation() const;
    [[nodiscard]] const QList<MediaTrack> &tracks() const;
    [[nodiscard]] const QList<MediaChapter> &chapters() const;
    [[nodiscard]] const QList<AudioDevice> &audioDevices() const;
    [[nodiscard]] QString selectedAudioDevice() const;
    // Navigation uses the latest accepted request until the backend confirms it.
    [[nodiscard]] int chapterNavigationIndex() const;
    [[nodiscard]] bool canStepFrameForward() const;
    [[nodiscard]] bool canStepFrameBackward() const;
    [[nodiscard]] bool canTakeScreenshot() const;
    [[nodiscard]] bool screenshotPending() const;
    bool takeScreenshot(const QString &absolutePath, bool includeSubtitles = true);
    // User preference; state().hardwareDecoder reports the decoder actually in use.
    [[nodiscard]] bool hardwareDecodingEnabled() const;
    [[nodiscard]] const SubtitlePreferences &subtitlePreferences() const;
    void setSubtitlePreferences(const SubtitlePreferences &preferences);
    // Normalized (trimmed, non-empty, capped) mpv alang/slang priorities.
    void setLanguagePreferences(const QStringList &audio, const QStringList &subtitle);
    void setAutoLoadExternalSubtitles(bool enabled);
    void restoreTrackPreference(const TrackSelectionPreference &preference);
    [[nodiscard]] bool trackRestorationPending() const;
    void applyAudioDevicePreference(const QString &name);
    // Session-only accepted intent; asynchronous errors are reported separately.
    [[nodiscard]] const VideoPresentation &videoPresentation() const;
    [[nodiscard]] const VideoFraming &videoFraming() const;
    [[nodiscard]] const VideoColorAdjustment &videoColorAdjustment() const;
    [[nodiscard]] HdrToneMapping hdrToneMapping() const;
    [[nodiscard]] DeinterlaceMode deinterlaceMode() const;
    void setVideoColorAdjustment(const VideoColorAdjustment &adjustment);
    void setHdrToneMapping(HdrToneMapping mapping);
    void setDeinterlaceMode(DeinterlaceMode mode);
    void setVideoAspect(VideoAspect aspect);
    [[nodiscard]] const AbLoop &abLoop() const;
    bool setLoopStart(double seconds);
    bool setLoopEnd(double seconds);
    bool clearAbLoop();

  public slots:
    void openFile(const QString &path);
    void openFileAtPosition(const QString &path, double seconds);
    void setPaused(bool paused);
    void togglePause();
    void stop();
    void setFileLoopEnabled(bool enabled);
    void setHardwareDecodingEnabled(bool enabled);
    void seekAbsolute(double seconds);
    void seekRelative(double seconds);
    void stepFrameForward();
    void stepFrameBackward();
    void selectChapter(int index);
    void nextChapter();
    void previousChapter();
    void setVolume(double volume);
    void setMuted(bool muted);
    void setAudioDevice(const QString &name);
    void setPlaybackSpeed(double multiplier);
    void adjustPlaybackSpeed(double delta);
    void setAudioDelay(double seconds);
    void adjustAudioDelay(double seconds);
    void setSubtitleDelay(double seconds);
    void adjustSubtitleDelay(double seconds);
    void selectAudioTrack(qint64 id);
    void selectSubtitleTrack(qint64 id);
    void addAudio(const QString &path);
    void addSubtitle(const QString &path);
    void setSubtitleVisible(bool visible);
    void setSubtitleScale(double scale);
    void adjustSubtitleScale(double delta);
    void setSubtitlePosition(double position);
    void adjustSubtitlePosition(double delta);
    void setSubtitleFont(const QString &font);
    void setVideoRotation(int clockwiseDegrees);
    void rotateVideoClockwise();
    void rotateVideoCounterClockwise();
    void setVideoZoom(double log2Scale);
    void adjustVideoZoom(double delta);
    void setVideoPanX(double offset);
    void adjustVideoPanX(double delta);
    void setVideoPanY(double offset);
    void adjustVideoPanY(double delta);
    void resetVideoFraming();
    void prepareForRendererReset();
    void resumeAfterRendererReset();

  signals:
    void fileOpenRequested();
    void stopRequested();
    void volumeRequested(double volume);
    void muteRequested(bool muted);
    void tuningRequested(double playbackSpeed, double audioDelay, double subtitleDelay);
    void hardwareDecodingPreferenceChanged(bool enabled);
    void subtitlePreferencesChanged();
    void videoPresentationChanged();
    void videoFramingChanged();
    void videoColorAdjustmentChanged();
    void hdrToneMappingChanged();
    void deinterlaceModeChanged();
    void abLoopChanged();
    void stateChanged();
    void mediaInformationChanged();
    void tuningChanged();
    void tracksChanged();
    void trackSelectionRequested(MediaTrack::Type type, qint64 id);
    void audioLoadRequested();
    void subtitleLoadRequested();
    void trackPreferenceRestoreFinished(bool canceled);
    void audioDevicesChanged();
    void audioDevicePreferenceChanged(const QString &name);
    void chaptersChanged();
    void chapterNavigationChanged();
    void fileLoaded();
    void playbackFinished();
    void screenshotPendingChanged();
    void screenshotFinished(const QString &path, bool success);
    void mediaLoadFailed(const QString &message);
    void errorOccurred(const QString &message);

  private:
    friend class MpvControllerTestAccess;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
