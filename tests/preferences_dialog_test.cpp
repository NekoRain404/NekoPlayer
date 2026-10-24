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
#include <optional>

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
        QCOMPARE(tabs->count(), 6);
        QCOMPARE(tabs->tabText(0), QStringLiteral("播放"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("音频"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("字幕"));
        QCOMPARE(tabs->tabText(3), QStringLiteral("媒体库"));
        QCOMPARE(tabs->tabText(4), QStringLiteral("外观"));
        QCOMPARE(tabs->tabText(5), QStringLiteral("交互"));
        QVERIFY(!tabs->tabIcon(0).isNull());
        QVERIFY(!tabs->tabIcon(1).isNull());
        QVERIFY(!tabs->tabIcon(2).isNull());
        QVERIFY(!tabs->tabIcon(3).isNull());
        QVERIFY(!tabs->tabIcon(5).isNull());
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

    void interactionCategoryExposesCurrentValues() {
        MpvController player(true, true);
        InteractionPreferences interaction;
        interaction.singleClickPlayPause = false;
        interaction.doubleClickFullscreen = false;
        interaction.middleClick = MiddleClickAction::Mute;
        interaction.wheelAdjustsVolume = false;
        interaction.wheelVolumeStep = 12;
        interaction.wheelSeeksTimeline = false;
        interaction.playlistSingleClick = true;
        PreferencesDialog dialog(player, nullptr, {}, true, 5, {}, interaction);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        auto *single =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionSingleClickChoice"));
        auto *doubleClick =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionDoubleClickChoice"));
        auto *middle =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionMiddleClickChoice"));
        auto *wheelVolume =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelVolumeChoice"));
        auto *wheelStep =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionWheelStepChoice"));
        auto *wheelSeek =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelSeekChoice"));
        auto *playlist =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionPlaylistClickChoice"));
        QVERIFY(tabs && single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist);
        QCOMPARE(tabs->count(), 6);
        QCOMPARE(tabs->tabText(5), QStringLiteral("交互"));
        QVERIFY(!single->isChecked());
        QVERIFY(!doubleClick->isChecked());
        QCOMPARE(middle->count(), 4);
        QCOMPARE(middle->currentData().toInt(), static_cast<int>(MiddleClickAction::Mute));
        QVERIFY(!wheelVolume->isChecked());
        QCOMPARE(wheelStep->currentData().toInt(), 12);
        QVERIFY(!wheelSeek->isChecked());
        QVERIFY(playlist->isChecked());
        QCOMPARE(single->accessibleName(), QStringLiteral("单击视频时播放或暂停"));
        QCOMPARE(middle->accessibleName(), QStringLiteral("中键单击操作"));
        QCOMPARE(wheelStep->accessibleName(), QStringLiteral("滚轮音量步长"));
    }

    void appliesInteractionPreferencesOnceWhenChanged() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *single =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionSingleClickChoice"));
        auto *doubleClick =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionDoubleClickChoice"));
        auto *middle =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionMiddleClickChoice"));
        auto *wheelVolume =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelVolumeChoice"));
        auto *wheelStep =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionWheelStepChoice"));
        auto *wheelSeek =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelSeekChoice"));
        auto *playlist =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionPlaylistClickChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist && buttons);
        std::optional<InteractionPreferences> applied;
        int emissions = 0;
        connect(&dialog, &PreferencesDialog::interactionPreferencesChanged, &dialog,
                [&applied, &emissions](InteractionPreferences preferences) {
                    applied = preferences;
                    ++emissions;
                });

        single->setChecked(false);
        doubleClick->setChecked(false);
        middle->setCurrentIndex(middle->findData(static_cast<int>(MiddleClickAction::Fullscreen)));
        wheelVolume->setChecked(false);
        wheelStep->setCurrentIndex(wheelStep->findData(9));
        wheelSeek->setChecked(false);
        playlist->setChecked(true);
        QCOMPARE(emissions, 0);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(emissions, 1);
        QVERIFY(applied.has_value());
        QVERIFY(!applied->singleClickPlayPause);
        QVERIFY(!applied->doubleClickFullscreen);
        QCOMPARE(applied->middleClick, MiddleClickAction::Fullscreen);
        QVERIFY(!applied->wheelAdjustsVolume);
        QCOMPARE(applied->wheelVolumeStep, 9);
        QVERIFY(!applied->wheelSeeksTimeline);
        QVERIFY(applied->playlistSingleClick);

        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(emissions, 1);
    }

    void interactionRestoreDefaultsResetsThisPage() {
        MpvController player(true, true);
        InteractionPreferences interaction;
        interaction.singleClickPlayPause = false;
        interaction.doubleClickFullscreen = false;
        interaction.middleClick = MiddleClickAction::Fullscreen;
        interaction.wheelAdjustsVolume = false;
        interaction.wheelVolumeStep = 17;
        interaction.wheelSeeksTimeline = false;
        interaction.playlistSingleClick = true;
        PreferencesDialog dialog(player, nullptr, {}, true, 5, {}, interaction);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        auto *single =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionSingleClickChoice"));
        auto *doubleClick =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionDoubleClickChoice"));
        auto *middle =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionMiddleClickChoice"));
        auto *wheelVolume =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelVolumeChoice"));
        auto *wheelStep =
            dialog.findChild<QComboBox *>(QStringLiteral("interactionWheelStepChoice"));
        auto *wheelSeek =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionWheelSeekChoice"));
        auto *playlist =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionPlaylistClickChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(tabs && single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist && buttons);
        dialog.selectCategory(PreferencesDialog::Category::Interaction);
        QCOMPARE(tabs->currentIndex(), 5);
        auto *restore = buttons->button(QDialogButtonBox::RestoreDefaults);
        QVERIFY(!restore->isHidden());
        restore->click();
        QVERIFY(single->isChecked());
        QVERIFY(doubleClick->isChecked());
        QCOMPARE(middle->currentData().toInt(), static_cast<int>(MiddleClickAction::None));
        QVERIFY(wheelVolume->isChecked());
        QCOMPARE(wheelStep->currentData().toInt(), 5);
        QVERIFY(wheelSeek->isChecked());
        QVERIFY(!playlist->isChecked());

        std::optional<InteractionPreferences> applied;
        connect(&dialog, &PreferencesDialog::interactionPreferencesChanged, &dialog,
                [&applied](InteractionPreferences preferences) { applied = preferences; });
        buttons->button(QDialogButtonBox::Apply)->click();
        QVERIFY(applied.has_value());
        QVERIFY(applied->singleClickPlayPause);
        QVERIFY(applied->doubleClickFullscreen);
        QCOMPARE(applied->middleClick, MiddleClickAction::None);
        QVERIFY(applied->wheelAdjustsVolume);
        QCOMPARE(applied->wheelVolumeStep, 5);
        QVERIFY(applied->wheelSeeksTimeline);
        QVERIFY(!applied->playlistSingleClick);
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
        const QList<QPair<QString, QString>> interactionNames{
            {QStringLiteral("interactionSingleClickChoice"),
             QStringLiteral("单击视频时播放或暂停")},
            {QStringLiteral("interactionDoubleClickChoice"), QStringLiteral("双击视频时切换全屏")},
            {QStringLiteral("interactionMiddleClickChoice"), QStringLiteral("中键单击操作")},
            {QStringLiteral("interactionWheelVolumeChoice"), QStringLiteral("滚轮调节音量")},
            {QStringLiteral("interactionWheelStepChoice"), QStringLiteral("滚轮音量步长")},
            {QStringLiteral("interactionWheelSeekChoice"), QStringLiteral("时间轴滚轮跳转")},
            {QStringLiteral("interactionPlaylistClickChoice"), QStringLiteral("播放列表单击播放")},
        };
        for (const auto &[objectName, name] : interactionNames) {
            auto *control = dialog.findChild<QWidget *>(objectName);
            QVERIFY2(control, qPrintable(objectName));
            QCOMPARE(control->accessibleName(), name);
        }
    }
};

QTEST_MAIN(PreferencesDialogTest)
#include "preferences_dialog_test.moc"
