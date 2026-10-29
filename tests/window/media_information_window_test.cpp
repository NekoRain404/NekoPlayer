#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/mediainformationdialog.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QtTest>

class MediaInformationWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void sharedActionReusesOneNonmodalDialog() {
        MainWindow window(true);
        auto *action = window.findChild<QAction *>(QStringLiteral("mediaInformationAction"));
        QVERIFY(action && window.actions().contains(action));
        QCOMPARE(action->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_F1));
        int menus = 0;
        for (auto *menu : window.findChildren<QMenu *>())
            if (menu->actions().contains(action))
                ++menus;
        QVERIFY(menus >= 2);
        for (auto *other : window.actions())
            if (other != action)
                QVERIFY(other->shortcut() != action->shortcut());
        action->trigger();
        auto *dialog = window.findChild<MediaInformationDialog *>();
        QVERIFY(dialog && dialog->isVisible() && !dialog->isModal());
        dialog->close();
        action->trigger();
        QCOMPARE(window.findChildren<MediaInformationDialog *>().size(), 1);
        QVERIFY(dialog->isVisible());
    }

    void reportFollowsMediaAndPreservesSelectionDuringProgress() {
        MpvController player(true, true);
        MediaInformationDialog dialog(player);
        auto *text = dialog.findChild<QPlainTextEdit *>(QStringLiteral("mediaInformationText"));
        auto *copy = dialog.findChild<QPushButton *>(QStringLiteral("copyMediaInformationButton"));
        QVERIFY(text && text->isReadOnly() && copy);
        QVERIFY(text->toPlainText().contains(QStringLiteral("未播放媒体")));
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(text->toPlainText().contains(QStringLiteral("320 × 180")));
        QTRY_VERIFY(text->toPlainText().contains(QStringLiteral("48000 Hz")));
        auto cursor = text->textCursor();
        cursor.setPosition(0);
        cursor.setPosition(2, QTextCursor::KeepAnchor);
        text->setTextCursor(cursor);
        const auto selected = cursor.selectedText();
        const auto position = player.state().position;
        QTRY_VERIFY(player.state().position > position + 0.1);
        QCOMPARE(text->textCursor().selectedText(), selected);
        player.stop();
        QTRY_VERIFY(text->toPlainText().contains(QStringLiteral("未播放媒体")));
        QVERIFY(!text->toPlainText().contains(QStringLiteral("mpeg4")));
    }
};

QTEST_MAIN(MediaInformationWindowTest)
#include "media_information_window_test.moc"
