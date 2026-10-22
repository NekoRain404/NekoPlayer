#pragma once

#include <QDialog>

class MpvController;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QTabWidget;

class PreferencesDialog final : public QDialog {
    Q_OBJECT
  public:
    enum class Category { Playback, Audio, Subtitles };

    explicit PreferencesDialog(MpvController &player, QWidget *parent = nullptr);
    void selectCategory(Category category);

  private:
    void apply();
    void updateApplyButton();
    void updateActualDecoder();
    void refreshAudioDevices();
    void displaySubtitlePreferences();
    MpvController &player_;
    QTabWidget *tabs_;
    QComboBox *hardwareDecoding_;
    QLabel *actualDecoder_;
    QComboBox *audioDevice_;
    QCheckBox *subtitleVisible_;
    QDoubleSpinBox *subtitleScale_;
    QDoubleSpinBox *subtitlePosition_;
    QFontComboBox *subtitleFont_;
    QPushButton *applyButton_;
    QPushButton *restoreDefaultsButton_;
    QString lastDecoder_;
    bool lastActive_ = false;
};
