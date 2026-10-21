#pragma once

#include "playback/subtitlepreferences.h"
#include <QDialog>

class MpvController;
class QCheckBox;
class QDoubleSpinBox;
class QFontComboBox;
class QPushButton;

class SubtitleSettingsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit SubtitleSettingsDialog(MpvController &player, QWidget *parent = nullptr);

  private:
    [[nodiscard]] SubtitlePreferences draft() const;
    void display(const SubtitlePreferences &preferences);
    void apply();
    void updateApplyButton();
    MpvController &player_;
    QCheckBox *visible_;
    QDoubleSpinBox *scale_;
    QDoubleSpinBox *position_;
    QFontComboBox *font_;
    QPushButton *applyButton_;
};
