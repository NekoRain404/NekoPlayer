#pragma once

#include "appearancepreferences.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QSlider;
class QLabel;

class PreferencesAppearancePage final : public QWidget {
    Q_OBJECT
  public:
    explicit PreferencesAppearancePage(const AppearancePreferences::Settings &baseline,
                                       QWidget *parent = nullptr);
    [[nodiscard]] AppearancePreferences::Settings draft() const;
    [[nodiscard]] bool changed() const;
    void restoreDefaults();
    void accept();

  signals:
    void edited();

  private:
    QComboBox *theme_;
    QComboBox *accent_;
    QCheckBox *glass_;
    QSlider *opacity_;
    QSlider *blur_;
    QLabel *opacityValue_;
    QLabel *blurValue_;
    AppearancePreferences::Settings baseline_;
};
