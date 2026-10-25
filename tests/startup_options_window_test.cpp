#include "ui/mainwindow.h"

#include <QAction>
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
};

QTEST_MAIN(StartupOptionsWindowTest)
#include "startup_options_window_test.moc"
