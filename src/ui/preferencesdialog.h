#pragma once

#include <QDialog>
#include "session/sessionstate.h"

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
    enum class Category { Playback, Audio, Subtitles, MediaLibrary };

    explicit PreferencesDialog(MpvController &player, QWidget *parent = nullptr,
                               MediaLibraryPreferences mediaLibrary = {},
                               bool resumePlaybackPosition = true);
    void selectCategory(Category category);

  signals:
    void mediaLibraryPreferencesChanged(bool scanningEnabled, bool recursiveByDefault);
    void resumePlaybackPositionChanged(bool enabled);

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
    QCheckBox *mediaScanningEnabled_;
    QCheckBox *recursiveFolderScan_;
    QCheckBox *resumePlaybackPosition_;
    MediaLibraryPreferences mediaLibraryPreferences_;
    bool resumePlaybackPositionDefault_ = true;
    QPushButton *applyButton_;
    QPushButton *restoreDefaultsButton_;
    QString lastDecoder_;
    bool lastActive_ = false;
};
