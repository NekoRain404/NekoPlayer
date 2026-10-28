#include "playback/mpvcontroller.h"
#include "ui/mainwindow.h"
#include "ui/navigationmenu.h"

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QTimer>
#include <QWidget>
#include <QtTest>

namespace {
QAction *action(MainWindow &window, const char *name) {
    return window.findChild<QAction *>(QString::fromLatin1(name));
}
} // namespace

// Compile-only coverage for the --fullscreen / --start-paused startup paths.
// This target is intentionally not registered with add_test: showing a window
// and loading media require a real Wayland session, which the test suite does
// not run. The one-shot pause is wired in the MainWindow constructor's
// fileLoaded/mediaLoadFailed/stopRequested connections.
class StartupOptionsWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void startPausedSeamIsCallable() {
        MainWindow window(true);
        window.setStartPaused(true);
        window.setStartPaused(false);
        window.setStartPaused(true);
    }

    void rejectedStartupOpenKeepsCompilationCovered() {
        MainWindow window(true);
        window.setStartPaused(true);
        // The pending-pause flag is private and the pause only becomes
        // observable after a real fileLoaded, so a rejected-then-accepted open
        // cannot be asserted in this compile-only target. This slot keeps the
        // synchronous rejection paths covered at compile time; the flag itself
        // is cleared there and is verified by inspection only.
        window.openFiles({});
        window.openFiles({QStringLiteral("http://")});
    }

    void fullscreenStartupPathEntersAndExits() {
        MainWindow window(true);
        auto *fullscreen = action(window, "fullscreenAction");
        QVERIFY(fullscreen);
        QVERIFY(fullscreen->isCheckable());
        // Mirrors main.cpp: show(), then the controlled enterFullscreen()
        // transition instead of a raw showFullScreen(). The controlled path
        // captures the normal geometry before fullscreen so the action can
        // restore it; raw showFullScreen() would encode fullscreen geometry
        // and make exit re-enter fullscreen.
        window.show();
        QVERIFY(window.isVisible());
        window.enterFullscreen();
        QVERIFY(window.isFullScreen());
        QVERIFY(fullscreen->isChecked());

        fullscreen->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(!fullscreen->isChecked());
        QVERIFY(window.isVisible());
    }

    void alwaysOnTopWhileFullscreenSurvivesExit() {
        MainWindow window(true);
        auto *fullscreen = action(window, "fullscreenAction");
        auto *top = action(window, "alwaysOnTopAction");
        QVERIFY(fullscreen && top);

        window.show();
        window.enterFullscreen();
        QVERIFY(window.isFullScreen());

        // Fullscreen owns the top hint while active; the user's choice must be
        // reapplied when the controlled exit restores the normal window.
        top->trigger();
        QVERIFY(top->isChecked());
        QVERIFY(window.isFullScreen());

        fullscreen->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(top->isChecked());
        QVERIFY(window.windowFlags().testFlag(Qt::WindowStaysOnTopHint));
        QVERIFY(window.isVisible());
    }

    void sleepTimerActionsArmAndCancelSessionTimer() {
        MpvController player(true, true);
        QWidget window;
        NavigationMenu navigation(player, &window);
        auto *off = window.findChild<QAction *>(QStringLiteral("sleepTimerOffAction"));
        auto *thirty = window.findChild<QAction *>(QStringLiteral("sleepTimer30Action"));
        auto *ninety = window.findChild<QAction *>(QStringLiteral("sleepTimer90Action"));
        auto *timer = window.findChild<QTimer *>(QStringLiteral("sleepTimer"));
        QVERIFY(off && thirty && ninety && timer);
        QVERIFY(off->isChecked() && !thirty->isChecked());
        QVERIFY(!timer->isActive());
        QSignalSpy feedback(&navigation, &NavigationMenu::feedback);
        thirty->trigger();
        QVERIFY(thirty->isChecked() && !off->isChecked());
        QVERIFY(timer->isActive());
        QCOMPARE(timer->interval(), 30 * 60 * 1000);
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(feedback.takeFirst().at(0).toString(),
                 QStringLiteral("睡眠定时器：30 分钟后停止"));
        ninety->trigger();
        QVERIFY(ninety->isChecked() && !thirty->isChecked());
        QCOMPARE(timer->interval(), 90 * 60 * 1000);
        QCOMPARE(feedback.count(), 1);
        feedback.clear();
        off->trigger();
        QVERIFY(off->isChecked() && !ninety->isChecked());
        QVERIFY(!timer->isActive());
        QCOMPARE(feedback.count(), 1);
        QCOMPARE(feedback.takeFirst().at(0).toString(), QStringLiteral("睡眠定时器已取消"));
        // Selecting 关闭 without a running timer stays silent.
        off->trigger();
        QVERIFY(feedback.isEmpty());
    }

    void sleepTimerExpiryStopsPlayback() {
        MpvController player(true, true);
        QWidget window;
        NavigationMenu navigation(player, &window);
        auto *off = window.findChild<QAction *>(QStringLiteral("sleepTimerOffAction"));
        auto *fifteen = window.findChild<QAction *>(QStringLiteral("sleepTimer15Action"));
        auto *timer = window.findChild<QTimer *>(QStringLiteral("sleepTimer"));
        QVERIFY(off && fifteen && timer);
        QSignalSpy stop(&player, &MpvController::stopRequested);
        QSignalSpy feedback(&navigation, &NavigationMenu::feedback);
        fifteen->trigger();
        QVERIFY(fifteen->isChecked() && timer->isActive());
        // The seam shortens the same single-shot timer; the menu owns it and
        // the timeout path is identical to a real duration.
        feedback.clear();
        navigation.setSleepTimerForTest(10);
        QVERIFY(timer->isActive());
        QTRY_COMPARE_WITH_TIMEOUT(stop.count(), 1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(feedback.count(), 1, 1000);
        QVERIFY(!timer->isActive());
        QVERIFY(off->isChecked() && !fifteen->isChecked());
        QCOMPARE(feedback.takeFirst().at(0).toString(), QStringLiteral("定时停止已触发"));
    }
};

QTEST_MAIN(StartupOptionsWindowTest)
#include "startup_options_window_test.moc"
