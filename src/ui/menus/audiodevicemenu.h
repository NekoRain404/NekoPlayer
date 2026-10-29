#pragma once

#include <QObject>

class MpvController;
class QActionGroup;
class QMenu;
class QWidget;

class AudioDeviceMenu final : public QObject {
    Q_OBJECT
  public:
    explicit AudioDeviceMenu(MpvController &controller, QWidget *parent);
    [[nodiscard]] QMenu *menu() const;

  private:
    void rebuild();
    void updateSelection();
    MpvController &controller_;
    QMenu *menu_;
    QActionGroup *group_;
};
