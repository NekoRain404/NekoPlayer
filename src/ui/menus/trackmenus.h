#pragma once

#include "playback/media/mediatrack.h"
#include <QObject>

class MpvController;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class TrackMenus final : public QObject {
    Q_OBJECT
  public:
    explicit TrackMenus(MpvController &controller, QWidget *window, QMenu *subtitleDelayMenu,
                        QMenu *subtitleDisplayMenu);
    [[nodiscard]] QMenu *audioMenu() const;
    [[nodiscard]] QMenu *subtitleMenu() const;

  private:
    void rebuild();
    void populate(QMenu *menu, QActionGroup *group, MediaTrack::Type type);
    void updateAvailability();
    void chooseExternalTrack(MediaTrack::Type type);
    MpvController &controller_;
    QWidget *window_;
    QMenu *audioMenu_;
    QMenu *subtitleMenu_;
    QMenu *subtitleDelayMenu_;
    QMenu *subtitleDisplayMenu_;
    QActionGroup *audioGroup_;
    QActionGroup *subtitleGroup_;
    QAction *loadAudioAction_;
    QAction *loadSubtitleAction_;
    quint64 sourceRevision_ = 0;
    bool active_ = false;
};
