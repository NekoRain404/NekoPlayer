#include "playback/mpvcontroller.h"
#include "ui/abloopmenu.h"
#include "ui/mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QtTest>

class AbLoopWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void sharedActionsFollowMediaAndLoopState() {
        MpvController player(true, true);
        QWidget window;
        AbLoopMenu loop(player, &window);
        auto *start = window.findChild<QAction *>(QStringLiteral("loopStartAction"));
        auto *end = window.findChild<QAction *>(QStringLiteral("loopEndAction"));
        auto *clear = window.findChild<QAction *>(QStringLiteral("clearAbLoopAction"));
        QVERIFY(start && end && clear);
        QVERIFY(!start->isEnabled() && !end->isEnabled() && !clear->isEnabled());
        QVERIFY(window.actions().contains(start) && window.actions().contains(end) &&
                window.actions().contains(clear));
        QCOMPARE(start->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
        QCOMPARE(end->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
        QCOMPARE(clear->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Backslash));
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy feedback(&loop, &AbLoopMenu::feedback);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTRY_VERIFY(start->isEnabled());
        QVERIFY(!end->isEnabled());
        player.seekAbsolute(0.4);
        QTRY_VERIFY(qAbs(player.state().position - 0.4) < 0.08);
        start->trigger();
        QVERIFY(player.abLoop().start.has_value());
        QVERIFY(end->isEnabled() && clear->isEnabled());
        player.seekAbsolute(2);
        QTRY_VERIFY(qAbs(player.state().position - 2) < 0.08);
        end->trigger();
        QVERIFY(player.abLoop().end.has_value());
        QVERIFY(*player.abLoop().end > *player.abLoop().start);
        QVERIFY(loop.menu()->title().contains(QStringLiteral("00:02")));
        QCOMPARE(feedback.count(), 2);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(player.state().paused);
        clear->trigger();
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QVERIFY(!end->isEnabled() && !clear->isEnabled());
        QVERIFY(player.setLoopStart(0.5));
        QVERIFY(end->isEnabled() && clear->isEnabled());
        player.stop();
        QTRY_VERIFY(!start->isEnabled() && !end->isEnabled() && !clear->isEnabled());
    }

    void mainWindowSharesNavigationSubmenu() {
        MainWindow window(true);
        auto *loop = window.findChild<QMenu *>(QStringLiteral("abLoopMenu"));
        auto *navigation = window.findChild<QMenu *>(QStringLiteral("navigationMenu"));
        QVERIFY(loop && navigation);
        QVERIFY(navigation->actions().contains(loop->menuAction()));
    }
};

QTEST_MAIN(AbLoopWindowTest)
#include "ab_loop_window_test.moc"
