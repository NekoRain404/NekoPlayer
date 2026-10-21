#pragma once

#include <QObject>

class MpvController;
class QMenu;
class QWidget;
class QAction;

class AbLoopMenu final : public QObject {
    Q_OBJECT
  public:
    AbLoopMenu(MpvController &player, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);

  private:
    void updateAvailability();
    void updatePresentation();
    MpvController &player_;
    QMenu *menu_;
    QAction *start_;
    QAction *end_;
    QAction *clear_;
    bool available_ = false;
};
