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
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QtTest>
#include <optional>

namespace {
QString backendText(MpvController &player, const char *property) {
    char *value = mpv_get_property_string(player.handle(), property);
    const auto text = QString::fromUtf8(value);
    mpv_free(value);
    return text;
}
} // namespace

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
        auto *seekStep = dialog.findChild<QSpinBox *>(QStringLiteral("seekStepChoice"));
        QVERIFY(seekStep);
        QCOMPARE(seekStep->value(), 5);
    }

    void formFieldsKeepOneSharedLabelColumnAcrossCategories() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        dialog.show();
        QCoreApplication::processEvents();

        QList<int> fieldColumnPositions;
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(tabs);
        for (int page = 0; page < tabs->count(); ++page) {
            tabs->setCurrentIndex(page);
            QCoreApplication::processEvents();
            for (auto *form : dialog.findChildren<QFormLayout *>()) {
                if (form->parentWidget() != tabs->widget(page))
                    continue;
                for (int row = 0; row < form->rowCount(); ++row) {
                    auto *labelItem = form->itemAt(row, QFormLayout::LabelRole);
                    auto *fieldItem = form->itemAt(row, QFormLayout::FieldRole);
                    auto *label = labelItem ? qobject_cast<QLabel *>(labelItem->widget()) : nullptr;
                    if (!label || !fieldItem || !label->isVisible())
                        continue;
                    fieldColumnPositions.append(
                        form->parentWidget()->mapTo(&dialog, fieldItem->geometry().topLeft()).x());
                    break;
                }
            }
        }

        QVERIFY(fieldColumnPositions.size() >= 5);
        for (const int position : fieldColumnPositions)
            QCOMPARE(position, fieldColumnPositions.constFirst());
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
        auto *seekStep = dialog.findChild<QSpinBox *>(QStringLiteral("seekStepChoice"));
        QVERIFY(seekStep);
        QCOMPARE(seekStep->minimum(), 1);
        QCOMPARE(seekStep->maximum(), 120);
        QCOMPARE(seekStep->value(), 5);
        QCOMPARE(seekStep->suffix(), QStringLiteral(" 秒"));
        QSignalSpy changed(&dialog, &PreferencesDialog::seekStepSecondsChanged);
        seekStep->setValue(7);
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(buttons);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().constFirst().toInt(), 7);
    }

    void subtitlesRestoreDefaultsKeepsSeekStep() {
        MpvController player(true, true);
        PreferencesDialog dialog(player, nullptr, {}, true, 7);
        auto *seekStep = dialog.findChild<QSpinBox *>(QStringLiteral("seekStepChoice"));
        auto *subtitleScale =
            dialog.findChild<QDoubleSpinBox *>(QStringLiteral("subtitleScaleChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(seekStep && subtitleScale && buttons);
        QCOMPARE(seekStep->value(), 7);
        subtitleScale->setValue(2.0);
        QSignalSpy changed(&dialog, &PreferencesDialog::seekStepSecondsChanged);
        dialog.selectCategory(PreferencesDialog::Category::Subtitles);
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(subtitleScale->value(), 1.0);
        QCOMPARE(seekStep->value(), 7);
        QVERIFY(changed.isEmpty());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(seekStep->value(), 7);
        QVERIFY(changed.isEmpty());
        // An unapplied Playback draft must also survive a Subtitles-page reset.
        seekStep->setValue(9);
        subtitleScale->setValue(2.0);
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(subtitleScale->value(), 1.0);
        QCOMPARE(seekStep->value(), 9);
        QVERIFY(changed.isEmpty());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.constFirst().constFirst().toInt(), 9);
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
        interaction.showRemainingTime = true;
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
        auto *remaining =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionRemainingTimeChoice"));
        QVERIFY(tabs && single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist && remaining);
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
        QVERIFY(remaining->isChecked());
        QCOMPARE(single->accessibleName(), QStringLiteral("单击视频时播放或暂停"));
        QCOMPARE(middle->accessibleName(), QStringLiteral("中键单击操作"));
        QCOMPARE(wheelStep->accessibleName(), QStringLiteral("音量步进"));
        QCOMPARE(remaining->accessibleName(), QStringLiteral("显示剩余时间"));
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
        auto *remaining =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionRemainingTimeChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist && remaining && buttons);
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
        remaining->setChecked(true);
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
        QVERIFY(applied->showRemainingTime);

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
        interaction.showRemainingTime = true;
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
        auto *remaining =
            dialog.findChild<QCheckBox *>(QStringLiteral("interactionRemainingTimeChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(tabs && single && doubleClick && middle && wheelVolume && wheelStep && wheelSeek &&
                playlist && remaining && buttons);
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
        QVERIFY(!remaining->isChecked());

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
        QVERIFY(!applied->showRemainingTime);
    }

    void languagePreferencesPopulateApplyAndReachPlayer() {
        MpvController player(true, true);
        LanguagePreferences languages;
        languages.audioLanguages = {QStringLiteral("jpn"), QStringLiteral("eng")};
        languages.subtitleLanguages = {QStringLiteral("chi")};
        PreferencesDialog dialog(player, nullptr, {}, true, 5, {}, {}, languages);
        auto *audio = dialog.findChild<QLineEdit *>(QStringLiteral("preferredAudioLanguagesEdit"));
        auto *subtitle =
            dialog.findChild<QLineEdit *>(QStringLiteral("preferredSubtitleLanguagesEdit"));
        auto *autoLoad =
            dialog.findChild<QCheckBox *>(QStringLiteral("autoLoadExternalSubtitlesCheck"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(audio && subtitle && autoLoad && buttons);
        QCOMPARE(audio->text(), QStringLiteral("jpn, eng"));
        QCOMPARE(audio->placeholderText(), QStringLiteral("例如：jpn,eng"));
        QCOMPARE(audio->accessibleName(), QStringLiteral("首选音轨语言"));
        QCOMPARE(subtitle->text(), QStringLiteral("chi"));
        QCOMPARE(subtitle->accessibleName(), QStringLiteral("首选字幕语言"));
        QCOMPARE(autoLoad->accessibleName(), QStringLiteral("自动加载同名字幕"));
        QVERIFY(!autoLoad->isChecked());
        QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());

        QSignalSpy changed(&dialog, &PreferencesDialog::languagePreferencesChanged);
        audio->setText(QStringLiteral(" eng , jpn, ,"));
        subtitle->setText(QStringLiteral("chi, "));
        autoLoad->setChecked(true);
        QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());
        QVERIFY(changed.isEmpty());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        const auto applied = qvariant_cast<LanguagePreferences>(changed.constFirst().constFirst());
        QCOMPARE(applied.audioLanguages,
                 (QStringList{QStringLiteral("eng"), QStringLiteral("jpn")}));
        QCOMPARE(applied.subtitleLanguages, (QStringList{QStringLiteral("chi")}));
        QVERIFY(applied.autoLoadExternalSubtitles);
        QTRY_COMPARE(backendText(player, "alang"), QStringLiteral("eng,jpn"));
        QTRY_COMPARE(backendText(player, "slang"), QStringLiteral("chi"));
        QTRY_COMPARE(backendText(player, "sub-auto"), QStringLiteral("exact"));
        QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
    }

    void languageRestoreDefaultsResetsOnlyVisibleCategory() {
        MpvController player(true, true);
        LanguagePreferences languages;
        languages.audioLanguages = {QStringLiteral("jpn")};
        languages.subtitleLanguages = {QStringLiteral("chi")};
        languages.autoLoadExternalSubtitles = true;
        PreferencesDialog dialog(player, nullptr, {}, true, 5, {}, {}, languages);
        auto *audio = dialog.findChild<QLineEdit *>(QStringLiteral("preferredAudioLanguagesEdit"));
        auto *subtitle =
            dialog.findChild<QLineEdit *>(QStringLiteral("preferredSubtitleLanguagesEdit"));
        auto *autoLoad =
            dialog.findChild<QCheckBox *>(QStringLiteral("autoLoadExternalSubtitlesCheck"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(audio && subtitle && autoLoad && buttons);
        auto *restore = buttons->button(QDialogButtonBox::RestoreDefaults);
        dialog.selectCategory(PreferencesDialog::Category::Audio);
        QVERIFY(!restore->isHidden());
        restore->click();
        QVERIFY(audio->text().isEmpty());
        QCOMPARE(subtitle->text(), QStringLiteral("chi"));
        QVERIFY(autoLoad->isChecked());
        dialog.selectCategory(PreferencesDialog::Category::Subtitles);
        restore->click();
        QVERIFY(subtitle->text().isEmpty());
        QVERIFY(!autoLoad->isChecked());
        QSignalSpy changed(&dialog, &PreferencesDialog::languagePreferencesChanged);
        buttons->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(changed.count(), 1);
        const auto applied = qvariant_cast<LanguagePreferences>(changed.constFirst().constFirst());
        QVERIFY(applied.audioLanguages.isEmpty());
        QVERIFY(applied.subtitleLanguages.isEmpty());
        QVERIFY(!applied.autoLoadExternalSubtitles);
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
        QCOMPARE(dialog.findChild<QSlider *>(QStringLiteral("appearanceGlassOpacityChoice"))
                     ->accessibleName(),
                 QStringLiteral("玻璃不透明度"));
        QCOMPARE(dialog.findChild<QSlider *>(QStringLiteral("appearanceGlassBlurChoice"))
                     ->accessibleName(),
                 QStringLiteral("玻璃模糊程度"));
        QCOMPARE(dialog.findChild<QFontComboBox *>(QStringLiteral("subtitleFontChoice"))
                     ->accessibleName(),
                 QStringLiteral("默认字幕字体"));
        const QList<QPair<QString, QString>> interactionNames{
            {QStringLiteral("interactionSingleClickChoice"),
             QStringLiteral("单击视频时播放或暂停")},
            {QStringLiteral("interactionDoubleClickChoice"), QStringLiteral("双击视频时切换全屏")},
            {QStringLiteral("interactionMiddleClickChoice"), QStringLiteral("中键单击操作")},
            {QStringLiteral("interactionWheelVolumeChoice"), QStringLiteral("滚轮调节音量")},
            {QStringLiteral("interactionWheelStepChoice"), QStringLiteral("音量步进")},
            {QStringLiteral("interactionWheelSeekChoice"), QStringLiteral("时间轴滚轮跳转")},
            {QStringLiteral("interactionPlaylistClickChoice"), QStringLiteral("播放列表单击播放")},
            {QStringLiteral("interactionRemainingTimeChoice"), QStringLiteral("显示剩余时间")},
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
