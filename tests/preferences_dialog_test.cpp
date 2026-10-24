#include "playback/mpvcontroller.h"
#include "ui/preferencesdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QIcon>
#include <QTabWidget>
#include <QtTest>

class PreferencesDialogTest final : public QObject {
    Q_OBJECT
  private slots:
    void exposesPotPlayerStyleSettingsCategories() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 4);
        QCOMPARE(tabs->tabText(0), QStringLiteral("播放"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("音频"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("字幕"));
        QCOMPARE(tabs->tabText(3), QStringLiteral("媒体库"));
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
        auto *scanning = dialog.findChild<QCheckBox *>(QStringLiteral("mediaScanningEnabledChoice"));
        auto *recursive = dialog.findChild<QCheckBox *>(QStringLiteral("recursiveFolderScanChoice"));
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
