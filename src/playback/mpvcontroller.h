#pragma once

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

  public slots:
    void openFile(const QString &path);
    void setPaused(bool paused);
    void togglePause();
    void stop();
    void seekAbsolute(double seconds);
    void seekRelative(double seconds);
    void setVolume(double volume);
    void setMuted(bool muted);
    void prepareForRendererReset();
    void resumeAfterRendererReset();

  signals:
    void stateChanged();
    void fileLoaded();
    void playbackFinished();
    void mediaLoadFailed(const QString &message);
    void errorOccurred(const QString &message);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
