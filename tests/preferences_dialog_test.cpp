#include "playback/mpvcontroller.h"
#include "ui/preferencesdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
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
        QCOMPARE(tabs->count(), 3);
        QCOMPARE(tabs->tabText(0), QStringLiteral("播放"));
        QCOMPARE(tabs->tabText(1), QStringLiteral("音频"));
        QCOMPARE(tabs->tabText(2), QStringLiteral("字幕"));
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("hardwareDecodingChoice")));
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("audioOutputDeviceChoice")));
        QVERIFY(dialog.findChild<QComboBox *>(QStringLiteral("subtitleFontChoice")));
    }

    void opensRequestedCategory() {
        MpvController player(true, true);
        PreferencesDialog dialog(player);
        dialog.selectCategory(PreferencesDialog::Category::Subtitles);
        auto *tabs = dialog.findChild<QTabWidget *>(QStringLiteral("preferencesCategories"));
        QVERIFY(tabs);
        QCOMPARE(tabs->currentIndex(), 2);
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
