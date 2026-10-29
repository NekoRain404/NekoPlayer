#include "playback/mpv/mpvcontroller.h"
#include "ui/menus/navigationmenu.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QtTest>
#include <cmath>

class FrameStepWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void sharedActionsFollowMediaAndDispatchFrameSteps() {
        MpvController player(true, true);
        QWidget window;
        NavigationMenu navigation(player, &window);
        auto *back = window.findChild<QAction *>(QStringLiteral("previousFrameAction"));
        auto *forward = window.findChild<QAction *>(QStringLiteral("nextFrameAction"));
        QVERIFY(back && forward);
        QVERIFY(!back->isEnabled() && !forward->isEnabled());
        QVERIFY(window.actions().contains(back) && window.actions().contains(forward));
        QVERIFY(navigation.menu()->actions().contains(back) &&
                navigation.menu()->actions().contains(forward));
        QCOMPARE(back->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Left));
        QCOMPARE(forward->shortcut(), QKeySequence(Qt::CTRL | Qt::Key_Right));
        QVERIFY(!back->autoRepeat() && !forward->autoRepeat());
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(back->isEnabled() && forward->isEnabled());
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.2);
        QTRY_VERIFY(std::abs(player.state().position - 1.2) < 0.005);
        forward->trigger();
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.005);
        QTRY_VERIFY(player.state().paused);
        back->trigger();
        QTRY_VERIFY(std::abs(player.state().position - 1.2) < 0.005);
        player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_VERIFY(!back->isEnabled() && !forward->isEnabled());
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QVERIFY(!back->isEnabled() && !forward->isEnabled());
    }

    void mainWindowKeepsExistingShortcutsAndRegistersFrameActions() {
        MainWindow window(true);
        auto *back = window.findChild<QAction *>(QStringLiteral("previousFrameAction"));
        auto *forward = window.findChild<QAction *>(QStringLiteral("nextFrameAction"));
        auto *navigation = window.findChild<QMenu *>(QStringLiteral("navigationMenu"));
        auto *subtitleEarlier =
            window.findChild<QAction *>(QStringLiteral("subtitleEarlierAction"));
        auto *subtitleLater = window.findChild<QAction *>(QStringLiteral("subtitleLaterAction"));
        QVERIFY(back && forward && navigation && subtitleEarlier && subtitleLater);
        QVERIFY(navigation->actions().contains(back) && navigation->actions().contains(forward));
        QCOMPARE(subtitleEarlier->shortcut(), QKeySequence(Qt::Key_Comma));
        QCOMPARE(subtitleLater->shortcut(), QKeySequence(Qt::Key_Period));
        for (auto *action : window.actions()) {
            if (action != back)
                QVERIFY(action->shortcut().isEmpty() || action->shortcut() != back->shortcut());
            if (action != forward)
                QVERIFY(action->shortcut().isEmpty() || action->shortcut() != forward->shortcut());
        }
    }
};

QTEST_MAIN(FrameStepWindowTest)
#include "frame_step_window_test.moc"
