#pragma once

#include "mediachapter.h"
#include "mediatrack.h"
#include <QList>
#include <QObject>
#include <QString>
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
    [[nodiscard]] const QList<MediaTrack> &tracks() const;
    [[nodiscard]] const QList<MediaChapter> &chapters() const;
    // Navigation uses the latest accepted request until the backend confirms it.
    [[nodiscard]] int chapterNavigationIndex() const;

  public slots:
    void openFile(const QString &path);
    void setPaused(bool paused);
    void togglePause();
    void stop();
    void setFileLoopEnabled(bool enabled);
    void seekAbsolute(double seconds);
    void seekRelative(double seconds);
    void selectChapter(int index);
    void nextChapter();
    void previousChapter();
    void setVolume(double volume);
    void setMuted(bool muted);
    void setPlaybackSpeed(double multiplier);
    void adjustPlaybackSpeed(double delta);
    void setAudioDelay(double seconds);
    void adjustAudioDelay(double seconds);
    void setSubtitleDelay(double seconds);
    void adjustSubtitleDelay(double seconds);
    void selectAudioTrack(qint64 id);
    void selectSubtitleTrack(qint64 id);
    void addSubtitle(const QString &path);
    void prepareForRendererReset();
    void resumeAfterRendererReset();

  signals:
    void volumeRequested(double volume);
    void muteRequested(bool muted);
    void stateChanged();
    void tuningChanged();
    void tracksChanged();
    void chaptersChanged();
    void chapterNavigationChanged();
    void fileLoaded();
    void playbackFinished();
    void mediaLoadFailed(const QString &message);
    void errorOccurred(const QString &message);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
