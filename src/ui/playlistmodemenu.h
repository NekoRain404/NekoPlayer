#pragma once

#include <QObject>

class PlaylistController;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class PlaylistModeMenu final : public QObject {
    Q_OBJECT
  public:
    explicit PlaylistModeMenu(PlaylistController &queue, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);

  private:
    void update();
    PlaylistController &queue_;
    QMenu *menu_;
    QActionGroup *group_;
    QAction *shuffleAction_;
};
