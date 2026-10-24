#include "playback/mpvcontroller.h"
#include "ui/appearancemenu.h"
#include "ui/preferencesdialog.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>

class PreferencesDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void themeMenuRetainsTransientAppearanceWithoutSaving() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QSettings store(directory.filePath(QStringLiteral("appearance.ini")), QSettings::IniFormat);
        AppearancePreferences::saveSettings(store, {AppearancePreferences::Theme::System,
                                                    AppearancePreferences::Accent::Amber, true});
        QWidget parent;
        AppearanceMenu menu(&parent, store, false);
        menu.setSettings(
            {AppearancePreferences::Theme::Dark, AppearancePreferences::Accent::Violet, false});
        auto *light = menu.menu()->findChild<QAction *>(QStringLiteral("themeLightAction"));
        QVERIFY(light);
        light->trigger();
        QCOMPARE(menu.settings().theme, AppearancePreferences::Theme::Light);
        QCOMPARE(menu.settings().accent, AppearancePreferences::Accent::Violet);
        QVERIFY(!menu.settings().glassEnabled);
        QVERIFY(light->isChecked());
        const auto saved = AppearancePreferences::loadSettings(store);
        QCOMPARE(saved.theme, AppearancePreferences::Theme::System);
        QCOMPARE(saved.accent, AppearancePreferences::Accent::Amber);
        QVERIFY(saved.glassEnabled);
        AppearanceMenu persistent(&parent, store, true);
        persistent.setSettings(
            {AppearancePreferences::Theme::Light, AppearancePreferences::Accent::Rose, false});
        const auto committed = AppearancePreferences::loadSettings(store);
        QCOMPARE(committed.theme, AppearancePreferences::Theme::Light);
        QCOMPARE(committed.accent, AppearancePreferences::Accent::Rose);
        QVERIFY(!committed.glassEnabled);
        QVERIFY(persistent.menu()
                    ->findChild<QAction *>(QStringLiteral("themeLightAction"))
                    ->isChecked());
    }

    void appearanceApplyCommitsBaselineAndDefaultsStayOnAppearance() {
        using Appearance = AppearancePreferences;
        MpvController player(true, true);
        PreferencesDialog dialog(player, nullptr, {}, true, 5,
                                 {Appearance::Theme::Light, Appearance::Accent::Amber, false});
        auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
        auto *accent = dialog.findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
        auto *glass = dialog.findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        QVERIFY(theme && accent && glass);
        QCOMPARE(theme->currentData().toInt(), 1);
        QCOMPARE(accent->currentData().toInt(), 4);
        QVERIFY(!glass->isChecked());
        for (int i = 0; i < 3; ++i)
            QCOMPARE(theme->itemData(i).toInt(), i);
        for (int i = 0; i < 5; ++i)
            QCOMPARE(accent->itemData(i).toInt(), i);
        QSignalSpy changed(&dialog, &PreferencesDialog::appearancePreferencesChanged);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        dialog.selectCategory(PreferencesDialog::Category::Appearance);
        QVERIFY(!buttons->button(QDialogButtonBox::RestoreDefaults)->isHidden());
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(theme->currentData().toInt(), 0);
        QCOMPARE(accent->currentData().toInt(), 0);
        QVERIFY(glass->isChecked());
        QVERIFY(changed.isEmpty());
        QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        const auto applied = qvariant_cast<Appearance::Settings>(changed.at(0).at(0));
        QCOMPARE(applied.theme, Appearance::Theme::Dark);
        QCOMPARE(applied.accent, Appearance::Accent::Ocean);
        QVERIFY(applied.glassEnabled);
        const auto committedStyle = qApp->styleSheet();
        const auto committedPalette = qApp->palette();
        QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
        accent->setCurrentIndex(2);
        dialog.reject();
        QCOMPARE(qApp->styleSheet(), committedStyle);
        QCOMPARE(qApp->palette(), committedPalette);
        QCOMPARE(changed.count(), 1);
        dialog.selectCategory(PreferencesDialog::Category::Playback);
        QVERIFY(buttons->button(QDialogButtonBox::RestoreDefaults)->isHidden());
    }

    void appearanceControlsPreviewAndCancel() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *theme = dialog.findChild<QComboBox *>(QStringLiteral("appearanceThemeChoice"));
        auto *accent = dialog.findChild<QComboBox *>(QStringLiteral("appearanceAccentChoice"));
        auto *glass = dialog.findChild<QCheckBox *>(QStringLiteral("appearanceGlassChoice"));
        QVERIFY(theme && accent && glass);
        QCOMPARE(theme->count(), 3);
        QCOMPARE(accent->count(), 5);
        QCOMPARE(theme->accessibleName(), QStringLiteral("基础主题"));
        QCOMPARE(accent->accessibleName(), QStringLiteral("强调色"));
        QCOMPARE(glass->accessibleName(), QStringLiteral("启用玻璃质感"));
        QVERIFY(glass->isChecked());
        const auto originalPalette = qApp->palette();
        const auto originalStyle = qApp->styleSheet();
        theme->setCurrentIndex(1);
        accent->setCurrentIndex(3);
        glass->setChecked(false);
        QVERIFY(qApp->palette() != originalPalette);
        QVERIFY(qApp->styleSheet() != originalStyle);
        dialog.reject();
        QCOMPARE(qApp->palette(), originalPalette);
        QCOMPARE(qApp->styleSheet(), originalStyle);
    }

    void exposesPotPlayerStyleSettingsCategories() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 5);
        QCOMPARE(tabs->tabText(0), QStringLiteral("播放"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("音频"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("字幕"));
        QCOMPARE(tabs->tabText(3), QStringLiteral("媒体库"));
        QCOMPARE(tabs->tabText(4), QStringLiteral("外观"));
        QVERIFY(!tabs->tabIcon(0).isNull());
        QVERIFY(!tabs->tabIcon(1).isNull());
        QVERIFY(!tabs->tabIcon(2).isNull());
        QVERIFY(!tabs->tabIcon(3).isNull());
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("hardwareDecodingChoice")));
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("audioOutputDeviceChoice")));
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("subtitleFontChoice")));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice")));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("recursiveFolderScanChoice")));
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice"))
                    ->isChecked());
        QVERIFY(dialog.findChild<QCheckBox *>(QStringLiteral("resumePlaybackPositionChoice"))
                    ->isChecked());
        auto *seekStep = dialog.findChild<QComboBox *>(QStringLiteral("seekStepChoice"));
        QVERIFY(seekStep);
        QCOMPARE(seekStep->currentData().toInt(), 5);
    }

    void opensRequestedCategory() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        dialog.selectCategory(PreferencesDialog::Category::Subtitles);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(tabs);
        QCOMPARE(tabs->currentIndex(), 2);
    }

    void appliesMediaLibraryPreferencesOnlyAfterApply() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *scanning =
            dialog.findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice"));
        auto *recursive =
            dialog.findChild<QCheckBox *>(QStringLiteral("recursiveFolderScanChoice"));
        QVERIFY(scanning && recursive);
        QSignalSpy changed(&dialog, &PreferencesDialog::mediaLibraryPreferencesChanged);
        scanning->setChecked(false);
        recursive->setChecked(true);
        QVERIFY(changed.isEmpty());
        dialog.selectCategory(PreferencesDialog::Category::MediaLibrary);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        const auto arguments = changed.takeFirst();
        QVERIFY(!arguments.at(0).toBool());
        QVERIFY(arguments.at(1).toBool());
    }

    void appliesResumePositionPreference() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *resume =
            dialog.findChild<QCheckBox *>(QStringLiteral("resumePlaybackPositionChoice"));
        QVERIFY(resume);
        QSignalSpy changed(&dialog, &PreferencesDialog::resumePlaybackPositionChanged);
        resume->setChecked(false);
        QVERIFY(changed.isEmpty());
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        QVERIFY(!changed.constFirst().constFirst().toBool());
    }

    void appliesSeekStepPreference() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *seekStep = dialog.findChild<QComboBox *>(QStringLiteral("seekStepChoice"));
        QVERIFY(seekStep);
        QCOMPARE(seekStep->count(), 3);
        QCOMPARE(seekStep->itemData(0).toInt(), 5);
        QCOMPARE(seekStep->itemData(1).toInt(), 10);
        QCOMPARE(seekStep->itemData(2).toInt(), 30);
        QSignalSpy changed(&dialog, &PreferencesDialog::seekStepSecondsChanged);
        seekStep->setCurrentIndex(1);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().constFirst().toInt(), 10);
    }

    void namesEveryPreferenceControlForAssistiveTechnology() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        QCOMPARE(dialog.findChild<QComboBox *>(QStringLiteral("audioOutputDeviceChoice"))
                     ->accessibleName(),
                 QStringLiteral("音频输出设备"));
        QCOMPARE(dialog.findChild<QCheckBox *>(QStringLiteral("subtitleVisibleChoice"))
                     ->accessibleName(),
                 QStringLiteral("显示字幕"));
        QCOMPARE(dialog.findChild<QDoubleSpinBox *>(QStringLiteral("subtitleScaleChoice"))
                     ->accessibleName(),
                 QStringLiteral("字幕缩放"));
        QCOMPARE(dialog.findChild<QDoubleSpinBox *>(QStringLiteral("subtitlePositionChoice"))
                     ->accessibleName(),
                 QStringLiteral("字幕垂直位置"));
        QCOMPARE(dialog.findChild<QFontComboBox *>(QStringLiteral("subtitleFontChoice"))
                     ->accessibleName(),
                 QStringLiteral("默认字幕字体"));
    }
};

QTEST_MAIN(PreferencesDialogTest)
#include "preferences_dialog_test.moc"
