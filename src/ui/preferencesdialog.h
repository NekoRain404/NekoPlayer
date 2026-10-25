#pragma once

#include "appearancepreferences.h"
#include "session/sessionstate.h"
#include <QDialog>

class MpvController;
class QComboBox;
class QDoubleSpinBox;
class QFontComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;

class PreferencesDialog final : public QDialog {
    Q_OBJECT
  public:
    enum class Category { Playback, Audio, Subtitles, MediaLibrary, Appearance, Interaction };

    explicit PreferencesDialog(MpvController &player, QWidget *parent = nullptr,
                               MediaLibraryPreferences mediaLibrary = {},
                               bool resumePlaybackPosition = true, int seekStepSeconds = 5,
                               AppearancePreferences::Settings appearance = {},
                               InteractionPreferences interaction = {},
                               LanguagePreferences languages = {});
    void selectCategory(Category category);

  signals:
    void mediaLibraryPreferencesChanged(bool scanningEnabled, bool recursiveByDefault);
    void resumePlaybackPositionChanged(bool enabled);
    void seekStepSecondsChanged(int seconds);
    void appearancePreferencesChanged(AppearancePreferences::Settings appearance);
    void interactionPreferencesChanged(InteractionPreferences preferences);
    void languagePreferencesChanged(LanguagePreferences preferences);

  private:
    void apply();
    void updateApplyButton();
    void updateActualDecoder();
    void refreshAudioDevices();
    void displaySubtitlePreferences();
    void createAppearancePage();
    void createInteractionPage(InteractionPreferences interaction);
    void displayInteractionPreferences(const InteractionPreferences &preferences);
    [[nodiscard]] InteractionPreferences interactionDraft() const;
    [[nodiscard]] bool interactionChanged() const;
    [[nodiscard]] LanguagePreferences languageDraft() const;
    [[nodiscard]] bool languageChanged() const;
    void previewAppearance();
    AppearancePreferences::Settings appearanceDraft() const;
    bool appearanceChanged() const;
    QComboBox *appearanceTheme_;
    QComboBox *appearanceAccent_;
    QCheckBox *appearanceGlass_;
    AppearancePreferences::Settings appearanceBaseline_;
    QPalette appearancePalette_;
    QString appearanceStyleSheet_;
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
    QSpinBox *seekStepSeconds_;
    MediaLibraryPreferences mediaLibraryPreferences_;
    bool resumePlaybackPositionDefault_ = true;
    int seekStepSecondsDefault_ = 5;
    InteractionPreferences interactionPreferences_;
    QCheckBox *interactionSingleClick_ = nullptr;
    QCheckBox *interactionDoubleClick_ = nullptr;
    QComboBox *interactionMiddleClick_ = nullptr;
    QCheckBox *interactionWheelVolume_ = nullptr;
    QComboBox *interactionWheelStep_ = nullptr;
    QCheckBox *interactionWheelSeek_ = nullptr;
    QCheckBox *interactionPlaylistClick_ = nullptr;
    QCheckBox *interactionRemainingTime_ = nullptr;
    LanguagePreferences languagePreferences_;
    QLineEdit *preferredAudioLanguages_ = nullptr;
    QLineEdit *preferredSubtitleLanguages_ = nullptr;
    QCheckBox *autoLoadExternalSubtitles_ = nullptr;
    QPushButton *applyButton_;
    QPushButton *restoreDefaultsButton_;
    QString lastDecoder_;
    bool lastActive_ = false;
    bool lastApplyActive_ = false;
};
