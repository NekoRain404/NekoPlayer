#pragma once

#include <QObject>

class MpvController;
class QAction;
class QActionGroup;
class QMenu;
class QWidget;

class VideoFramingMenu final : public QObject {
    Q_OBJECT
  public:
    explicit VideoFramingMenu(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *menu() const;

  signals:
    void feedback(const QString &message);

  private:
    void update();
    MpvController &controller_;
    QMenu *menu_;
    QActionGroup *presets_;
    QAction *zoomIn_;
    QAction *zoomOut_;
    QAction *panLeft_;
    QAction *panRight_;
    QAction *panUp_;
    QAction *panDown_;
    QAction *reset_;
};
