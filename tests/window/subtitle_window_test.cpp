#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/preferencesdialog.h"
#include "ui/menus/subtitledisplaymenu.h"

#include <QAction>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontComboBox>
#include <QMenu>
#include <QPushButton>
#include <QtTest>

class SubtitleWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void dialogUsesDraftApplyAndCancel() {
        MpvController player(true, true);
        {
            PreferencesDialog dialog(player);
            dialog.selectCategory(PreferencesDialog::Category::Subtitles);
            auto *visible = dialog.findChild<QCheckBox *>(QStringLiteral("subtitleVisibleChoice"));
            auto *scale = dialog.findChild<QDoubleSpinBox *>(QStringLiteral("subtitleScaleChoice"));
            auto *position =
                dialog.findChild<QDoubleSpinBox *>(QStringLiteral("subtitlePositionChoice"));
            auto *font = dialog.findChild<QFontComboBox *>(QStringLiteral("subtitleFontChoice"));
            auto *buttons =
                dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
            QVERIFY(visible && scale && position && font && buttons);
            QCOMPARE(font->currentText(), QStringLiteral("sans-serif"));
            QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
            visible->setChecked(false);
            scale->setValue(1.5);
            position->setValue(84);
            font->setEditText(QStringLiteral("unavailable family 猫"));
            QVERIFY(player.subtitlePreferences().visible);
            QVERIFY(buttons->button(QDialogButtonBox::Apply)->isEnabled());
            buttons->button(QDialogButtonBox::Apply)->click();
            QVERIFY(!player.subtitlePreferences().visible);
            QCOMPARE(player.subtitlePreferences().scale, 1.5);
            QCOMPARE(player.subtitlePreferences().position, 84.0);
            QCOMPARE(player.subtitlePreferences().font, QStringLiteral("unavailable family 猫"));
            QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
            scale->setValue(2.0);
            buttons->button(QDialogButtonBox::Cancel)->click();
            QCOMPARE(player.subtitlePreferences().scale, 1.5);
        }
        PreferencesDialog dialog(player);
        dialog.selectCategory(PreferencesDialog::Category::Subtitles);
        auto *font = dialog.findChild<QFontComboBox *>(QStringLiteral("subtitleFontChoice"));
        auto *buttons = dialog.findChild<QDialogButtonBox *>(QStringLiteral("preferencesButtons"));
        QVERIFY(font && buttons);
        QCOMPARE(font->currentText(), QStringLiteral("unavailable family 猫"));
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        QCOMPARE(player.subtitlePreferences().scale, 1.5);
        QCOMPARE(font->currentText(), QStringLiteral("sans-serif"));
        buttons->button(QDialogButtonBox::Ok)->click();
        QVERIFY(player.subtitlePreferences().visible);
        QCOMPARE(player.subtitlePreferences().scale, 1.0);
        QCOMPARE(player.subtitlePreferences().position, 100.0);
        QCOMPARE(player.subtitlePreferences().font, QStringLiteral("sans-serif"));
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
    }

    void sharedActionsUseLatestIntentWhileIdle() {
        MpvController player(true, true);
        QWidget window;
        SubtitleDisplayMenu display(player, &window);
        const auto action = [&window](const char *name) {
            return window.findChild<QAction *>(QString::fromLatin1(name));
        };
        auto *visible = action("subtitleVisibleAction");
        auto *larger = action("subtitleScaleUpAction");
        auto *up = action("subtitleMoveUpAction");
        QVERIFY(visible && larger && up);
        QVERIFY(display.menu()->actions().contains(display.settingsAction()));
        QCOMPARE(display.settingsAction()->shortcut(), QKeySequence(Qt::Key_F6));
        QVERIFY(visible->isChecked());
        QCOMPARE(visible->shortcut(), QKeySequence(Qt::Key_V));
        visible->trigger();
        QVERIFY(!player.subtitlePreferences().visible && !visible->isChecked());
        player.setSubtitleVisible(true);
        QVERIFY(visible->isChecked());
        for (int step = 0; step < 5; ++step) {
            larger->trigger();
            up->trigger();
        }
        QCOMPARE(player.subtitlePreferences().scale, 1.5);
        QCOMPARE(player.subtitlePreferences().position, 90.0);
        QVERIFY(!player.state().active);
    }
};

QTEST_MAIN(SubtitleWindowTest)
#include "subtitle_window_test.moc"
