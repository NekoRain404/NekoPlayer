#include "playback/mpvcontroller.h"
#include "ui/preferencesdialog.h"

#include <QComboBox>
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
};

QTEST_MAIN(PreferencesDialogTest)
#include "preferences_dialog_test.moc"
