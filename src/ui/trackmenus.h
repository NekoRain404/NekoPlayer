#pragma once

#include "playback/mediatrack.h"
#include <QObject>

class MpvController;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class TrackMenus final : public QObject {
    Q_OBJECT
  public:
    explicit TrackMenus(MpvController &controller, QWidget *window, QMenu *subtitleDelayMenu);
    [[nodiscard]] QMenu *audioMenu() const;
    [[nodiscard]] QMenu *subtitleMenu() const;

  private:
    void rebuild();
    void populate(QMenu *menu, QActionGroup *group, MediaTrack::Type type);
    void updateAvailability();
    void chooseSubtitle();
    MpvController &controller_;
    QWidget *window_;
    QMenu *audioMenu_;
    QMenu *subtitleMenu_;
    QMenu *subtitleDelayMenu_;
    QActionGroup *audioGroup_;
    QActionGroup *subtitleGroup_;
    QAction *loadSubtitleAction_;
    bool active_ = false;
};
