#pragma once

#include <QObject>
#include <functional>

class MpvController;
enum class EqualizerPreset;
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
    [[nodiscard]] QMenu *equalizerMenu() const;
    void retranslate();

  signals:
    void feedback(const QString &message);

  private:
    void createSpeedMenu();
    void createDelayMenu(QMenu *menu, bool audio);
    void createEqualizerMenu();
    void restoreEqualizer();
    void saveEqualizer(EqualizerPreset preset);
    void update();
    void chooseValue(bool speed, bool audio = false);
    void announceAfterChange();
    MpvController &controller_;
    QWidget *window_;
    QMenu *speedMenu_;
    QMenu *audioDelayMenu_;
    QMenu *subtitleDelayMenu_;
    QMenu *equalizerMenu_;
    QActionGroup *speedGroup_;
    QActionGroup *equalizerGroup_;
    bool announce_ = false;
};
