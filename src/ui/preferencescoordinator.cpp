#include "preferencescoordinator.h"
#include "playback/mpvcontroller.h"

#include <utility>

PreferencesCoordinator::PreferencesCoordinator(MpvController &player, Handlers handlers,
                                               QObject *parent)
    : QObject(parent), player_(player), handlers_(std::move(handlers)) {}

std::unique_ptr<PreferencesDialog>
PreferencesCoordinator::createDialog(QWidget *parent, PreferencesDialog::Category category) {
    const MediaLibraryPreferences mediaLibrary =
        handlers_.mediaLibrary ? handlers_.mediaLibrary() : MediaLibraryPreferences{};
    const bool resumePlaybackPosition =
        handlers_.resumePlaybackPosition ? handlers_.resumePlaybackPosition() : true;
    const int seekStepSeconds = handlers_.seekStepSeconds ? handlers_.seekStepSeconds() : 5;
    const AppearancePreferences::Settings appearance =
        handlers_.appearance ? handlers_.appearance() : AppearancePreferences::Settings{};
    const InteractionPreferences interaction =
        handlers_.interaction ? handlers_.interaction() : InteractionPreferences{};
    const LanguagePreferences language =
        handlers_.language ? handlers_.language() : LanguagePreferences{};

    auto dialog =
        std::make_unique<PreferencesDialog>(player_, parent, mediaLibrary, resumePlaybackPosition,
                                            seekStepSeconds, appearance, interaction, language);

    connect(dialog.get(), &PreferencesDialog::appearancePreferencesChanged, dialog.get(),
            [handler = handlers_.setAppearance](AppearancePreferences::Settings settings) {
                if (handler)
                    handler(settings);
            });
    connect(dialog.get(), &PreferencesDialog::mediaLibraryPreferencesChanged, dialog.get(),
            [handler = handlers_.setMediaLibrary](bool scanningEnabled, bool recursiveByDefault) {
                if (handler)
                    handler(MediaLibraryPreferences{scanningEnabled, recursiveByDefault});
            });
    connect(dialog.get(), &PreferencesDialog::resumePlaybackPositionChanged, dialog.get(),
            [handler = handlers_.setResumePlaybackPosition](bool enabled) {
                if (handler)
                    handler(enabled);
            });
    connect(dialog.get(), &PreferencesDialog::seekStepSecondsChanged, dialog.get(),
            [handler = handlers_.setSeekStepSeconds](int seconds) {
                if (handler)
                    handler(seconds);
            });
    connect(dialog.get(), &PreferencesDialog::interactionPreferencesChanged, dialog.get(),
            [handler = handlers_.setInteraction](InteractionPreferences preferences) {
                if (handler)
                    handler(preferences);
            });
    connect(dialog.get(), &PreferencesDialog::languagePreferencesChanged, dialog.get(),
            [handler = handlers_.setLanguage](LanguagePreferences preferences) {
                if (handler)
                    handler(preferences);
            });

    dialog->selectCategory(category);
    return dialog;
}

void PreferencesCoordinator::show(QWidget *parent, PreferencesDialog::Category category) {
    createDialog(parent, category)->exec();
}
