#pragma once

#include <QObject>
#include <functional>

class MpvController;
class QActionGroup;
class QMenu;
class QWidget;

class TuningMenus final : public QObject {
    Q_OBJECT
  public:
    explicit TuningMenus(MpvController &controller, QWidget *window);
    [[nodiscard]] QMenu *speedMenu() const;
    [[nodiscard]] QMenu *audioDelayMenu() const;
    [[nodiscard]] QMenu *subtitleDelayMenu() const;
    void retranslate();

  signals:
    void feedback(const QString &message);

  private:
    void createSpeedMenu();
    void createDelayMenu(QMenu *menu, bool audio);
    void update();
    void chooseValue(bool speed, bool audio = false);
    void announceAfterChange();
    MpvController &controller_;
    QWidget *window_;
    QMenu *speedMenu_;
    QMenu *audioDelayMenu_;
    QMenu *subtitleDelayMenu_;
    QActionGroup *speedGroup_;
    bool announce_ = false;
};
