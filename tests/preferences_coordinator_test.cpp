#include "playback/mpvcontroller.h"
#include "ui/appearancemenu.h"
#include "ui/preferencescoordinator.h"
#include "ui/preferencesdialog.h"
#include "ui/subtitledisplaymenu.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QWidget>
#include <QtTest>
#include <optional>

namespace {
struct PreferencesState {
    AppearancePreferences::Settings appearance;
    MediaLibraryPreferences mediaLibrary;
    bool resumePlaybackPosition = true;
    int seekStepSeconds = 5;
    InteractionPreferences interaction;
};

std::unique_ptr<PreferencesCoordinator> coordinatorFor(MpvController &player,
                                                       PreferencesState &state) {
    return std::make_unique<PreferencesCoordinator>(
        player,
        PreferencesCoordinator::Handlers{
            .appearance = [&state] { return state.appearance; },
            .setAppearance =
                [&state](AppearancePreferences::Settings appearance) {
                    state.appearance = appearance;
                },
            .mediaLibrary = [&state] { return state.mediaLibrary; },
            .setMediaLibrary =
                [&state](MediaLibraryPreferences preferences) { state.mediaLibrary = preferences; },
            .resumePlaybackPosition = [&state] { return state.resumePlaybackPosition; },
            .setResumePlaybackPosition =
                [&state](bool enabled) { state.resumePlaybackPosition = enabled; },
            .seekStepSeconds = [&state] { return state.seekStepSeconds; },
            .setSeekStepSeconds = [&state](int seconds) { state.seekStepSeconds = seconds; },
            .interaction = [&state] { return state.interaction; },
            .setInteraction =
                [&state](InteractionPreferences preferences) { state.interaction = preferences; }},
        nullptr);
}
} // namespace

class PreferencesCoordinatorTest final : public QObject {
    Q_OBJECT
  private slots:
    void currentValuesReachTheDialog() {
        using Appearance = AppearancePreferences;
        MpvController player(true, true);
        PreferencesState state;
        state.appearance = {Appearance::Theme::Light, Appearance::Accent::Amber, false};
        state.mediaLibrary = {false, true};
        state.resumePlaybackPosition = false;
        state.seekStepSeconds = 10;
        state.interaction.singleClickPlayPause = false;
        state.interaction.middleClick = MiddleClickAction::Fullscreen;
        state.interaction.wheelVolumeStep = 8;
        state.interaction.playlistSingleClick = true;
        state.interaction.showRemainingTime = true;
        auto coordinator = coordinatorFor(player, state);
        auto dialog = coordinator->createDialog(nullptr, PreferencesDialog::Category::MediaLibrary);
        QVERIFY(dialog);

        auto *theme = dialog->findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
        auto *accent = dialog->findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
        auto *glass = dialog->findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        auto *scanning =
            dialog->findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice"));
        auto *recursive =
            dialog->findChild<QCheckBox *>(QStringLiteral("recursiveFolderScanChoice"));
        auto *resume =
            dialog->findChild<QCheckBox *>(QStringLiteral("resumePlaybackPositionChoice"));
        auto *seek = dialog->findChild<QSpinBox *>(QStringLiteral("seekStepChoice"));
        auto *single =
            dialog->findChild<QCheckBox *>(QStringLiteral("interactionSingleClickChoice"));
        auto *middle =
            dialog->findChild<QComboBox *>(QStringLiteral("interactionMiddleClickChoice"));
        auto *wheelStep =
            dialog->findChild<QComboBox *>(QStringLiteral("interactionWheelStepChoice"));
        auto *playlist =
            dialog->findChild<QCheckBox *>(QStringLiteral("interactionPlaylistClickChoice"));
        auto *remaining =
            dialog->findChild<QCheckBox *>(QStringLiteral("interactionRemainingTimeChoice"));
        auto *tabs = dialog->findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(theme && accent && glass && scanning && recursive && resume && seek && single &&
                middle && wheelStep && playlist && remaining && tabs);

        QCOMPARE(theme->currentData().toInt(), static_cast<int>(Appearance::Theme::Light));
        QCOMPARE(accent->currentData().toInt(), static_cast<int>(Appearance::Accent::Amber));
        QVERIFY(!glass->isChecked());
        QVERIFY(!scanning->isChecked());
        QVERIFY(recursive->isChecked());
        QVERIFY(!resume->isChecked());
        QCOMPARE(seek->value(), 10);
        QVERIFY(!single->isChecked());
        QCOMPARE(middle->currentData().toInt(), static_cast<int>(MiddleClickAction::Fullscreen));
        QCOMPARE(wheelStep->currentData().toInt(), 8);
        QVERIFY(playlist->isChecked());
        QVERIFY(remaining->isChecked());
        QCOMPARE(tabs->currentIndex(), static_cast<int>(PreferencesDialog::Category::MediaLibrary));
    }

    void applyPropagatesEveryPreferenceThroughHandlers() {
        using Appearance = AppearancePreferences;
        MpvController player(true, true);
        PreferencesState state;
        const auto originalPalette = qApp->palette();
        const auto originalStyle = qApp->styleSheet();
        {
            auto coordinator = coordinatorFor(player, state);
            auto dialog =
                coordinator->createDialog(nullptr, PreferencesDialog::Category::Appearance);

            auto *theme = dialog->findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
            auto *accent = dialog->findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
            auto *glass = dialog->findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
            auto *scanning =
                dialog->findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice"));
            auto *recursive =
                dialog->findChild<QCheckBox *>(QStringLiteral("recursiveFolderScanChoice"));
            auto *resume =
                dialog->findChild<QCheckBox *>(QStringLiteral("resumePlaybackPositionChoice"));
            auto *seek = dialog->findChild<QSpinBox *>(QStringLiteral("seekStepChoice"));
            auto *single =
                dialog->findChild<QCheckBox *>(QStringLiteral("interactionSingleClickChoice"));
            auto *middle =
                dialog->findChild<QComboBox *>(QStringLiteral("interactionMiddleClickChoice"));
            auto *wheelVolume =
                dialog->findChild<QCheckBox *>(QStringLiteral("interactionWheelVolumeChoice"));
            auto *wheelStep =
                dialog->findChild<QComboBox *>(QStringLiteral("interactionWheelStepChoice"));
            auto *wheelSeek =
                dialog->findChild<QCheckBox *>(QStringLiteral("interactionWheelSeekChoice"));
            auto *playlist =
                dialog->findChild<QCheckBox *>(QStringLiteral("interactionPlaylistClickChoice"));
            auto *remaining =
                dialog->findChild<QCheckBox *>(QStringLiteral("interactionRemainingTimeChoice"));
            auto *buttons =
                dialog->findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
            QVERIFY(theme && accent && glass && scanning && recursive && resume && seek && single &&
                    middle && wheelVolume && wheelStep && wheelSeek && playlist && remaining &&
                    buttons);

            theme->setCurrentIndex(theme->findData(static_cast<int>(Appearance::Theme::Light)));
            accent->setCurrentIndex(accent->findData(static_cast<int>(Appearance::Accent::Amber)));
            glass->setChecked(false);
            scanning->setChecked(false);
            recursive->setChecked(true);
            resume->setChecked(false);
            seek->setValue(10);
            single->setChecked(false);
            middle->setCurrentIndex(middle->findData(static_cast<int>(MiddleClickAction::Mute)));
            wheelVolume->setChecked(false);
            wheelStep->setCurrentIndex(wheelStep->findData(13));
            wheelSeek->setChecked(false);
            playlist->setChecked(true);
            remaining->setChecked(true);
            buttons->button(QDialogButtonBox::Apply)->click();

            QCOMPARE(state.appearance.theme, Appearance::Theme::Light);
            QCOMPARE(state.appearance.accent, Appearance::Accent::Amber);
            QVERIFY(!state.appearance.glassEnabled);
            QVERIFY(!state.mediaLibrary.scanningEnabled);
            QVERIFY(state.mediaLibrary.recursiveByDefault);
            QVERIFY(!state.resumePlaybackPosition);
            QCOMPARE(state.seekStepSeconds, 10);
            QVERIFY(!state.interaction.singleClickPlayPause);
            QCOMPARE(state.interaction.middleClick, MiddleClickAction::Mute);
            QVERIFY(!state.interaction.wheelAdjustsVolume);
            QCOMPARE(state.interaction.wheelVolumeStep, 13);
            QVERIFY(!state.interaction.wheelSeeksTimeline);
            QVERIFY(state.interaction.playlistSingleClick);
            QVERIFY(state.interaction.showRemainingTime);
        }
        qApp->setPalette(originalPalette);
        qApp->setStyleSheet(originalStyle);
    }

    void appearanceApplyPersistsThroughTheAppearanceOwner() {
        using Appearance = AppearancePreferences;
        MpvController player(true, true);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto settingsPath = directory.filePath(QStringLiteral("appearance.ini"));
        QSettings store(settingsPath, QSettings::IniFormat);
        QWidget parent;
        AppearanceMenu menu(&parent, store, true);
        const auto originalPalette = qApp->palette();
        const auto originalStyle = qApp->styleSheet();
        {
            PreferencesCoordinator coordinator(
                player,
                {.appearance = [&menu] { return menu.settings(); },
                 .setAppearance =
                     [&menu](Appearance::Settings appearance) { menu.setSettings(appearance); },
                 .mediaLibrary = [] { return MediaLibraryPreferences{}; },
                 .setMediaLibrary = [](MediaLibraryPreferences) {},
                 .resumePlaybackPosition = [] { return true; },
                 .setResumePlaybackPosition = [](bool) {},
                 .seekStepSeconds = [] { return 5; },
                 .setSeekStepSeconds = [](int) {},
                 .interaction = [] { return InteractionPreferences{}; },
                 .setInteraction = [](InteractionPreferences) {}},
                nullptr);
            auto dialog =
                coordinator.createDialog(nullptr, PreferencesDialog::Category::Appearance);
            auto *theme = dialog->findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
            auto *accent = dialog->findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
            auto *glass = dialog->findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
            auto *buttons =
                dialog->findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
            QVERIFY(theme && accent && glass && buttons);

            // Changing a control previews the draft; Apply must leave the
            // committed value with the single persistence owner.
            theme->setCurrentIndex(theme->findData(static_cast<int>(Appearance::Theme::Light)));
            accent->setCurrentIndex(accent->findData(static_cast<int>(Appearance::Accent::Amber)));
            glass->setChecked(false);
            buttons->button(QDialogButtonBox::Apply)->click();

            QCOMPARE(menu.settings().theme, Appearance::Theme::Light);
            QCOMPARE(menu.settings().accent, Appearance::Accent::Amber);
            QVERIFY(!menu.settings().glassEnabled);
            QSettings persisted(settingsPath, QSettings::IniFormat);
            const auto saved = AppearancePreferences::loadSettings(persisted);
            QCOMPARE(saved.theme, Appearance::Theme::Light);
            QCOMPARE(saved.accent, Appearance::Accent::Amber);
            QVERIFY(!saved.glassEnabled);
        }
        qApp->setPalette(originalPalette);
        qApp->setStyleSheet(originalStyle);
    }

    void f6InvokesTheSharedPreferencesCallback() {
        MpvController player(true, true);
        QWidget window;
        std::optional<PreferencesDialog::Category> invoked;
        SubtitleDisplayMenu display(
            player, &window,
            [&invoked](PreferencesDialog::Category category) { invoked = category; });
        auto *settings = display.settingsAction();
        QVERIFY(settings);
        QCOMPARE(settings->shortcut(), QKeySequence(Qt::Key_F6));
        QVERIFY(settings->isEnabled());
        settings->trigger();
        QVERIFY(invoked.has_value());
        QCOMPARE(*invoked, PreferencesDialog::Category::Subtitles);
    }
};

QTEST_MAIN(PreferencesCoordinatorTest)
#include "preferences_coordinator_test.moc"
