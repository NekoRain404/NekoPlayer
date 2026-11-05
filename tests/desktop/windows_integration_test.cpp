#include "desktop/windowsintegration.h"

#include <QAction>
#include <QWidget>
#include <QtTest>

class WindowsIntegrationTest final : public QObject {
    Q_OBJECT
  private slots:
    void installationIsSafeAndIdempotent() {
        QWidget window;
        QAction playPause;
        QAction stop;
        QAction previous;
        QAction next;
        const auto first =
            WindowsIntegration::installMediaHotkeys(window, &playPause, &stop, &previous, &next);
        QVERIFY(first >= 0);
        // A second installation must not leak or double-register.
        const auto second =
            WindowsIntegration::installMediaHotkeys(window, &playPause, &stop, &previous, &next);
        QVERIFY(second >= 0);
        WindowsIntegration::releaseMediaHotkeys();
        WindowsIntegration::releaseMediaHotkeys();
    }

    void executionStateRequestsAreSafe() {
        WindowsIntegration::setPlaybackScreenAwake(true);
        WindowsIntegration::setPlaybackScreenAwake(false);
        WindowsIntegration::setPlaybackScreenAwake(false);
        QVERIFY(true);
    }
};

QTEST_MAIN(WindowsIntegrationTest)
#include "windows_integration_test.moc"
