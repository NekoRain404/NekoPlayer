#include "playback/videowidget.h"
#include "ui/mainwindow.h"

#include <QAction>
#include <QStatusBar>
#include <QtTest>

class FullscreenFeedbackWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void windowedActionsKeepUsingTheStatusBar() {
        MainWindow window(true);
        auto *forward = window.findChild<QAction *>(QStringLiteral("seekForwardAction"));
        QVERIFY(forward);

        forward->trigger();

        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("前进 5 秒"));
    }

    void fullscreenActionsReplaceTheOverlayMessage() {
        MainWindow window(true);
        auto *overlay = window.findChild<QWidget *>(QStringLiteral("actionFeedbackOverlay"));
        auto *forward = window.findChild<QAction *>(QStringLiteral("seekForwardAction"));
        auto *back = window.findChild<QAction *>(QStringLiteral("seekBackAction"));
        QVERIFY(overlay && forward && back);

        window.showFullScreen();
        QTRY_VERIFY(window.isFullScreen());
        forward->trigger();
        QVERIFY(overlay->isVisible());
        QCOMPARE(overlay->accessibleDescription(), QStringLiteral("前进 5 秒"));
        back->trigger();
        QCOMPARE(overlay->accessibleDescription(), QStringLiteral("后退 5 秒"));
        QVERIFY(overlay->testAttribute(Qt::WA_TransparentForMouseEvents));
    }
};

QTEST_MAIN(FullscreenFeedbackWindowTest)
#include "fullscreen_feedback_window_test.moc"
