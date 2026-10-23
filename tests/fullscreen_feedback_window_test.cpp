#include "playback/mpvcontroller.h"
#include "playback/videowidget.h"
#include "ui/mainwindow.h"

#include <QAction>
#include <QLabel>
#include <QMenuBar>
#include <QProgressBar>
#include <QSlider>
#include <QStatusBar>
#include <QTest>
#include <QtTest>

class FullscreenFeedbackWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void tabActionTogglesIndependentPlaybackInformation() {
        MainWindow window(true);
        auto *action = window.findChild<QAction *>(QStringLiteral("playbackInformationAction"));
        auto *info = window.findChild<QLabel *>(QStringLiteral("playbackInformationOverlay"));
        QVERIFY(action && info);
        QCOMPARE(action->shortcut(), QKeySequence(Qt::Key_Tab));
        QVERIFY(action->isCheckable());
        QVERIFY(!action->autoRepeat());
        window.show();
        action->trigger();
        QVERIFY(info->isVisible());
        QCOMPARE(info->text(), QStringLiteral("未播放媒体"));
        QCOMPARE(info->textFormat(), Qt::PlainText);
        action->trigger();
        QVERIFY(!info->isVisible());
    }

    void fullscreenSeekShowsSeparateProgressWithHiddenTransport() {
        MainWindow window(true);
        window.show();
        window.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active &&
                    window.controller()->state().duration > 0);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        window.controller()->seekAbsolute(2);
        QTRY_VERIFY(qAbs(window.controller()->state().position - 2) < 0.1);
        auto *fullscreen = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *back = window.findChild<QAction *>(QStringLiteral("seekBackAction"));
        auto *infoAction = window.findChild<QAction *>(QStringLiteral("playbackInformationAction"));
        auto *panel = window.findChild<QWidget *>(QStringLiteral("seekProgressOverlay"));
        auto *progress = window.findChild<QProgressBar *>(QStringLiteral("seekProgressBar"));
        auto *info = window.findChild<QLabel *>(QStringLiteral("playbackInformationOverlay"));
        auto *transport = window.findChild<QWidget *>(QStringLiteral("transportBar"));
        QVERIFY(fullscreen && back && infoAction && panel && progress && info && transport);
        fullscreen->trigger();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(transport->isVisible());
        infoAction->trigger();
        back->trigger();
        QVERIFY(panel->isVisible());
        QVERIFY(info->isVisible());
        QVERIFY(transport->isVisible());
        QTRY_COMPARE(progress->value(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(!panel->isVisible(), 3500);
        QVERIFY(info->isVisible());
        QVERIFY(window.controller()->state().paused);
        infoAction->trigger();
        QVERIFY(!info->isVisible());
    }

    void fullscreenClearsTimelinePreview() {
        MainWindow window(true);
        window.show();
        auto *fullscreen = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *preview = window.findChild<QWidget *>(QStringLiteral("thumbnailPreview"));
        QVERIFY(fullscreen && preview);
        preview->show();
        QVERIFY(preview->isVisible());

        fullscreen->trigger();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(!preview->isVisible());
    }

    void miniPlayerClearsTimelinePreview() {
        MainWindow window(true);
        window.show();
        auto *mini = window.findChild<QAction *>(QStringLiteral("miniPlayerAction"));
        auto *preview = window.findChild<QWidget *>(QStringLiteral("thumbnailPreview"));
        QVERIFY(mini && preview);
        preview->show();
        QVERIFY(preview->isVisible());

        mini->trigger();
        QTRY_VERIFY(mini->isChecked());
        QVERIFY(!preview->isVisible());
    }

    void timelinePressClearsTimelinePreview() {
        MainWindow window(true);
        window.show();
        window.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(window.controller()->state().active &&
                    window.controller()->state().duration > 0);
        auto *slider = window.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        auto *preview = window.findChild<QWidget *>(QStringLiteral("thumbnailPreview"));
        QVERIFY(slider && preview);
        slider->resize(400, 24);
        preview->show();
        QVERIFY(preview->isVisible());

        QTest::mousePress(slider, Qt::LeftButton, Qt::NoModifier, QPoint(200, 12));
        QVERIFY(!preview->isVisible());
        QTest::mouseRelease(slider, Qt::LeftButton, Qt::NoModifier, QPoint(200, 12));
    }

    void playbackFinishedClearsTimelinePreview() {
        MainWindow window(true);
        window.show();
        auto *preview = window.findChild<QWidget *>(QStringLiteral("thumbnailPreview"));
        QVERIFY(preview);
        preview->show();
        QVERIFY(preview->isVisible());

        QVERIFY(QMetaObject::invokeMethod(window.controller(), "playbackFinished",
                                          Qt::DirectConnection));
        QVERIFY(!preview->isVisible());
    }

    void miniPlayerAndFullscreenRemainMutuallyExclusive() {
        MainWindow window(true);
        window.show();
        auto *mini = window.findChild<QAction *>(QStringLiteral("miniPlayerAction"));
        auto *fullscreen = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *playlistAction = window.findChild<QAction *>(QStringLiteral("showPlaylistAction"));
        QVERIFY(mini && fullscreen && playlistAction);

        mini->trigger();
        QTRY_VERIFY(mini->isChecked());
        fullscreen->trigger();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(!mini->isChecked());
        QVERIFY(!playlistAction->isEnabled());

        fullscreen->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(!mini->isChecked());
        QVERIFY(playlistAction->isEnabled());
    }

    void fullscreenAndMiniPlayerSwitchCleanlyInBothDirections() {
        MainWindow window(true);
        window.show();
        auto *mini = window.findChild<QAction *>(QStringLiteral("miniPlayerAction"));
        auto *fullscreen = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *playlistAction = window.findChild<QAction *>(QStringLiteral("showPlaylistAction"));
        QVERIFY(mini && fullscreen && playlistAction);

        fullscreen->trigger();
        QTRY_VERIFY(window.isFullScreen());
        mini->trigger();
        QTRY_VERIFY(mini->isChecked());
        QVERIFY(!window.isFullScreen());
        QVERIFY(window.windowFlags().testFlag(Qt::Tool));
        QVERIFY(!fullscreen->isChecked());

        mini->trigger();
        QTRY_VERIFY(!mini->isChecked());
        QVERIFY(!window.windowFlags().testFlag(Qt::Tool));
        QVERIFY(playlistAction->isEnabled());
    }

    void fullscreenEntryRevealsTransportBeforeItFades() {
        MainWindow window(true);
        window.show();
        auto *fullscreen = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *transport = window.findChild<QWidget *>(QStringLiteral("transportBar"));
        QVERIFY(fullscreen && transport);

        fullscreen->trigger();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(transport->isVisible());
    }

    void externalWindowStateChangesKeepFullscreenUiInSync() {
        MainWindow window(true);
        window.show();
        auto *playlistAction = window.findChild<QAction *>(QStringLiteral("showPlaylistAction"));
        auto *fullscreenAction = window.findChild<QAction *>(QStringLiteral("fullscreenAction"));
        auto *playlist = window.findChild<QWidget *>(QStringLiteral("playlistPane"));
        auto *transport = window.findChild<QWidget *>(QStringLiteral("transportBar"));
        QVERIFY(playlistAction && fullscreenAction && playlist && transport);

        playlistAction->trigger();
        QTRY_VERIFY(playlist->isVisible());
        window.showFullScreen();
        QTRY_VERIFY(window.isFullScreen());
        QVERIFY(fullscreenAction->isChecked());
        QVERIFY(!window.menuBar()->isVisible());
        QVERIFY(!window.statusBar()->isVisible());
        QVERIFY(!playlistAction->isEnabled());
        QVERIFY(!playlist->isVisible());
        QVERIFY(transport->isVisible());

        window.showNormal();
        QTRY_VERIFY(!window.isFullScreen());
        QVERIFY(!fullscreenAction->isChecked());
        QVERIFY(window.menuBar()->isVisible());
        QVERIFY(window.statusBar()->isVisible());
        QVERIFY(playlistAction->isEnabled());
        QVERIFY(playlist->isVisible());
    }

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
