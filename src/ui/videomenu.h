#pragma once

#include <QObject>

class MpvController;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class VideoMenu final : public QObject {
    Q_OBJECT
  public:
    explicit VideoMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);

  private:
    void update();
    MpvController &controller_;
    QMenu *menu_;
    QActionGroup *aspects_;
    QAction *resetRotation_;
};
