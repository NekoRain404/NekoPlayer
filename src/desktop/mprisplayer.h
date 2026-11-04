#pragma once

#include <QObject>
#include <QVariantMap>
#include <memory>

class MpvController;
class PlaylistController;
class QWidget;

// Exposes NekoPlayer on the session bus as org.mpris.MediaPlayer2.NekoPlayer.
// The controller/playlist remain the source of truth; this object only
// translates MPRIS properties and commands and never talks to libmpv.
class MprisPlayer final : public QObject {
    Q_OBJECT
  public:
    MprisPlayer(MpvController &player, PlaylistController &playlist, QObject *parent = nullptr);
    ~MprisPlayer() override;

    [[nodiscard]] bool isRegistered() const;
    void setWindow(QWidget *window);
    void setWindowVisible(bool visible);

    [[nodiscard]] QString playbackStatus() const;
    [[nodiscard]] QVariantMap metadata() const;
    [[nodiscard]] qlonglong position() const;
    [[nodiscard]] double rate() const;
    [[nodiscard]] double volume() const;
    [[nodiscard]] double minimumRate() const;
    [[nodiscard]] double maximumRate() const;
    [[nodiscard]] bool canSeek() const;
    [[nodiscard]] bool canGoNext() const;
    [[nodiscard]] bool canGoPrevious() const;
    [[nodiscard]] bool shuffle() const;
    [[nodiscard]] QString loopStatus() const;

    void setRate(double value);
    void setVolume(double value);
    void setShuffle(bool enabled);
    void setLoopStatus(const QString &status);

    void raise();
    void quit();
    void play();
    void pause();
    void playPause();
    void stop();
    void next();
    void previous();
    void seekRelative(qlonglong offsetMicroseconds);
    void setPosition(const QString &trackId, qlonglong positionMicroseconds);
    void openUri(const QString &uri);

  signals:
    void openUriRequested(const QString &uri);
    void raiseRequested();
    void quitRequested();
    void seeked(qlonglong positionMicroseconds);

  private:
    void refresh();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
