#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "ui/mainwindow.h"
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QSlider>
#include <QStatusBar>
#include <QtTest>

namespace {
QString mediaPath() { return QString::fromUtf8(TEST_MEDIA_PATH); }
bool hasColoredVideo(const QImage &image) {
    if (image.isNull())
        return false;
    int colored = 0;
    for (int y = image.height() / 4; y < image.height() * 3 / 4; y += 8) {
        for (int x = image.width() / 4; x < image.width() * 3 / 4; x += 8) {
            const auto color = image.pixelColor(x, y);
            if (qMax(color.red(), qMax(color.green(), color.blue())) -
                    qMin(color.red(), qMin(color.green(), color.blue())) >
                80)
                ++colored;
        }
    }
    return colored > 40;
}
QAction *action(MainWindow &window, const char *name) {
    return window.findChild<QAction *>(QString::fromLatin1(name));
}
} // namespace

class WindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void rendersVideo() {
        MainWindow window(true);
        window.show();
        // Wayland compositors can suspend an unfocused window without exposing
        // it. Renderer readiness and actual framebuffer pixels are the contract.
        QTRY_VERIFY_WITH_TIMEOUT(window.videoWidget()->isReady(), 3000);
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_VERIFY_WITH_TIMEOUT(hasColoredVideo(window.videoWidget()->grabFramebuffer()), 5000);
        const auto frame = window.videoWidget()->grabFramebuffer();
        const QSize expectedFramebufferSize(
            qRound(window.videoWidget()->width() * window.videoWidget()->devicePixelRatioF()),
            qRound(window.videoWidget()->height() * window.videoWidget()->devicePixelRatioF()));
        QCOMPARE(frame.size(), expectedFramebufferSize);
        auto *pause = action(window, "pauseAction");
        QVERIFY(pause);
        pause->trigger();
        QTRY_VERIFY(window.controller()->state().paused);
        auto *timeline = window.findChild<QSlider *>(QStringLiteral("timelineSlider"));
        QVERIFY(timeline && timeline->isEnabled());
        timeline->setValue(5000);
        QVERIFY(QMetaObject::invokeMethod(timeline, "sliderReleased", Qt::DirectConnection));
        QTRY_VERIFY(qAbs(window.controller()->state().position - 2.0) < 0.3);
        timeline->setSliderDown(true);
        timeline->setSliderPosition(7500);
        auto *time = window.findChild<QLabel *>(QStringLiteral("timeLabel"));
        QVERIFY(time);
        QTRY_VERIFY(time->text().startsWith(QStringLiteral("00:00:03")));
        timeline->setSliderDown(false);
        window.resize(800, 520);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        const auto size = window.size();
        const auto verifyFrameGeometry = [&window] {
            QTRY_VERIFY_WITH_TIMEOUT(hasColoredVideo(window.videoWidget()->grabFramebuffer()),
                                     3000);
            const auto frame = window.videoWidget()->grabFramebuffer();
            const QSize expected(
                qRound(window.videoWidget()->width() * window.videoWidget()->devicePixelRatioF()),
                qRound(window.videoWidget()->height() * window.videoWidget()->devicePixelRatioF()));
            QCOMPARE(frame.size(), expected);
        };
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(window.isFullScreen());
        verifyFrameGeometry();
        // The compositor may suspend an unfocused test surface, so shortcut
        // dispatch cannot be exercised here. Test the shared QAction instead.
        action(window, "exitFullscreenAction")->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        QTRY_COMPARE(window.size(), size);
        verifyFrameGeometry();
        QTest::mouseDClick(window.videoWidget(), Qt::LeftButton);
        QTRY_VERIFY(window.isFullScreen());
        verifyFrameGeometry();
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(!window.isFullScreen());
        verifyFrameGeometry();
        pause->trigger();
        QTRY_VERIFY(!window.controller()->state().paused);
        window.close();
    }

    void appendsLocalDropsAndRecoversFromInvalidInput() {
        MainWindow window(true);
        window.show();
        QTRY_VERIFY(window.videoWidget()->isReady());
        QSignalSpy errors(&window, &MainWindow::userError);
        const auto drop = [&window](const QList<QUrl> &urls) {
            QMimeData mime;
            mime.setUrls(urls);
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
            QDragEnterEvent enter(QPointF(50, 50), Qt::CopyAction, &mime,
#else
            QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &mime,
#endif
                                  Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(&window, &enter);
            QDropEvent event(QPointF(50, 50), Qt::CopyAction, &mime, Qt::LeftButton,
                             Qt::NoModifier);
            QApplication::sendEvent(&window, &event);
        };
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        drop({QUrl::fromLocalFile(mediaPath()), QUrl::fromLocalFile(mediaPath())});
        QCOMPARE(window.playlist()->model()->rowCount(), 3);
        QVERIFY(errors.isEmpty());
        QVERIFY(window.controller()->state().active);
        drop({QUrl(QStringLiteral("https://example.com/movie.mkv"))});
        QTRY_COMPARE(errors.count(), 1);
        action(window, "fullscreenAction")->trigger();
        QTRY_VERIFY(window.isFullScreen());
        window.openFile(QStringLiteral("/no-such-neko-file.mkv"));
        QTRY_COMPARE(errors.count(), 2);
        QTRY_VERIFY_WITH_TIMEOUT(!window.statusBar()->currentMessage().isEmpty(), 3000);
        QTRY_VERIFY(!window.controller()->state().active);
        drop({QUrl::fromLocalFile(mediaPath())});
        QTRY_VERIFY(window.controller()->state().active);
    }

    void pausedAppendUpdatesNavigationImmediately() {
        MainWindow window(true);
        window.show();
        window.openFile(mediaPath());
        QTRY_VERIFY(window.controller()->state().active);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        QVERIFY(!action(window, "nextAction")->isEnabled());
        window.appendFiles({mediaPath()});
        // No mpv property event is needed to publish a queue-only change.
        QVERIFY(action(window, "nextAction")->isEnabled());
        QVERIFY(window.controller()->state().paused);
        QCOMPARE(window.playlist()->model()->currentIndex(), 0);
        action(window, "clearPlaylistAction")->trigger();
        QVERIFY(!action(window, "clearPlaylistAction")->isEnabled());
        QVERIFY(!action(window, "nextAction")->isEnabled());
    }

    void commandLineMultipleFilesStartsPlaylist() {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"), QStringLiteral("generic"));
        process.setProcessEnvironment(environment);
        process.start(QString::fromUtf8(PLAYER_EXECUTABLE),
                      {QStringLiteral("--silent-audio"), QStringLiteral("--smoke-test"),
                       mediaPath(), mediaPath()});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    void queuedOpenAndClose() {
        for (int i = 0; i < 2; ++i) {
            MainWindow window(true);
            window.openFile(mediaPath());
            window.show();
            QTRY_VERIFY(window.controller()->state().active);
            window.controller()->setPaused(true);
            QTRY_VERIFY(window.controller()->state().paused);
            window.hide();
            QTest::qWait(120);
            window.show();
            QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        }
    }

    void recreatesGraphicsContext_data() {
        QTest::addColumn<QString>("path");
        QTest::addColumn<bool>("paused");
        QTest::newRow("audio-paused") << mediaPath() << true;
        QTest::newRow("audio-playing") << mediaPath() << false;
        QTest::newRow("video-only") << QString::fromUtf8(TEST_VIDEO_ONLY_PATH) << true;
    }

    void recreatesGraphicsContext() {
        QFETCH(QString, path);
        QFETCH(bool, paused);
        MainWindow window(true);
        QSignalSpy errors(&window, &MainWindow::userError);
        window.show();
        window.openFile(path);
        QTRY_VERIFY(window.controller()->state().active && window.controller()->state().seekable);
        window.controller()->setPaused(true);
        QTRY_VERIFY(window.controller()->state().paused);
        window.controller()->seekAbsolute(1.5);
        QTRY_VERIFY(qAbs(window.controller()->state().position - 1.5) < 0.2);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        if (!paused) {
            window.controller()->setPaused(false);
            QTRY_VERIFY(!window.controller()->state().paused);
        }
        QSignalSpy recreated(window.videoWidget(), &VideoWidget::ready);
        QSignalSpy loaded(window.controller(), &MpvController::fileLoaded);
        QWidget temporaryHost;
        auto *content = window.takeCentralWidget();
        content->setParent(&temporaryHost);
        temporaryHost.resize(900, 600);
        content->resize(900, 600);
        temporaryHost.show();
        content->show();
        QTRY_VERIFY(!recreated.isEmpty());
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_COMPARE(window.controller()->state().paused, paused);
        QTRY_VERIFY(window.controller()->state().position >= 1.3);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.front().front().toString()));
        window.setCentralWidget(content);
        content->show();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(window.videoWidget()->isReady());
        QTRY_VERIFY(window.controller()->state().active);
        QTRY_COMPARE(window.controller()->state().paused, paused);
        QTRY_VERIFY(window.controller()->state().position >= 1.3);
        QTRY_VERIFY(hasColoredVideo(window.videoWidget()->grabFramebuffer()));
        QVERIFY2(errors.isEmpty(),
                 qPrintable(errors.isEmpty() ? QString() : errors.front().front().toString()));
    }

    void reportsMissingGraphicsContext() {
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        // Use Qt's minimal platform so this test is independent of whether the
        // parent test process is running on native Wayland or X11/XWayland.
        // The minimal plugin cannot provide the OpenGL surface required by
        // QOpenGLWidget, which exercises the application's failure message.
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("minimal"));
        environment.insert(QStringLiteral("QT_QPA_PLATFORMTHEME"), QStringLiteral("generic"));
        environment.insert(QStringLiteral("QT_OPENGL"), QStringLiteral("software"));
        process.setProcessEnvironment(environment);
        process.start(
            QString::fromUtf8(PLAYER_EXECUTABLE),
            {QStringLiteral("--silent-audio"), QStringLiteral("--smoke-test"), mediaPath()});
        QVERIFY(process.waitForStarted());
        QVERIFY(process.waitForFinished(6000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 1);
        const auto output = QString::fromUtf8(process.readAllStandardError());
        QVERIFY2(output.contains(QStringLiteral("OpenGL 初始化失败")), qPrintable(output));
    }
};
QTEST_MAIN(WindowTest)
#include "window_test.moc"
