#include "playback/mpvcontroller.h"
#include "ui/preferencescoordinator.h"
#include "ui/preferencesdialog.h"
#include "ui/subtitledisplaymenu.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QWidget>
#include <QtTest>
#include <optional>

namespace {
struct PreferencesState {
    AppearancePreferences::Settings appearance;
    MediaLibraryPreferences mediaLibrary;
    bool resumePlaybackPosition = true;
    int seekStepSeconds = 5;
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
            .setSeekStepSeconds = [&state](int seconds) { state.seekStepSeconds = seconds; }},
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
        auto *seek = dialog->findChild<QComboBox *>(QStringLiteral("seekStepChoice"));
        auto *tabs = dialog->findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(theme && accent && glass && scanning && recursive && resume && seek && tabs);

        QCOMPARE(theme->currentData().toInt(), static_cast<int>(Appearance::Theme::Light));
        QCOMPARE(accent->currentData().toInt(), static_cast<int>(Appearance::Accent::Amber));
        QVERIFY(!glass->isChecked());
        QVERIFY(!scanning->isChecked());
        QVERIFY(recursive->isChecked());
        QVERIFY(!resume->isChecked());
        QCOMPARE(seek->currentData().toInt(), 10);
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
            auto *seek = dialog->findChild<QComboBox *>(QStringLiteral("seekStepChoice"));
            auto *buttons =
                dialog->findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
            QVERIFY(theme && accent && glass && scanning && recursive && resume && seek && buttons);

            theme->setCurrentIndex(theme->findData(static_cast<int>(Appearance::Theme::Light)));
            accent->setCurrentIndex(accent->findData(static_cast<int>(Appearance::Accent::Amber)));
            glass->setChecked(false);
            scanning->setChecked(false);
            recursive->setChecked(true);
            resume->setChecked(false);
            seek->setCurrentIndex(seek->findData(10));
            buttons->button(QDialogButtonBox::Apply)->click();

            QCOMPARE(state.appearance.theme, Appearance::Theme::Light);
            QCOMPARE(state.appearance.accent, Appearance::Accent::Amber);
            QVERIFY(!state.appearance.glassEnabled);
            QVERIFY(!state.mediaLibrary.scanningEnabled);
            QVERIFY(state.mediaLibrary.recursiveByDefault);
            QVERIFY(!state.resumePlaybackPosition);
            QCOMPARE(state.seekStepSeconds, 10);
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
