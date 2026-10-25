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

    void fullscreenStartupPathIsCallable() {
        MainWindow window(true);
        auto *fullscreen = action(window, "fullscreenAction");
        QVERIFY(fullscreen);
        QVERIFY(fullscreen->isCheckable());
        // main.cpp calls show() then showFullScreen(); the existing
        // WindowStateChange path synchronizes the action and presentation.
        window.show();
        window.showFullScreen();
        window.showNormal();
    }
};

QTEST_MAIN(StartupOptionsWindowTest)
#include "startup_options_window_test.moc"
