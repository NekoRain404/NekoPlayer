#include "playback/mpv/mpvcontroller.h"
#include "ui/menus/videomenu.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QtTest>

class VideoWindowTest final : public QObject {
    Q_OBJECT
  private slots:
    void rejectedAspectActionRestoresChecks() {
        MpvController player(true, true);
        QWidget window;
        VideoMenu video(player, &window);
        auto *source = window.findChild<QAction *>(QStringLiteral("videoAspectSourceAction"));
        auto *wide = window.findChild<QAction *>(QStringLiteral("videoAspect16x9Action"));
        QVERIFY(source && wide);
        QTest::qWait(50);
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        wide->trigger();
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Source);
        QVERIFY(source->isChecked() && !wide->isChecked());
    }

    void actionsShareControllerIntentAndExclusiveChecks() {
        MpvController player(true, true);
        QWidget window;
        VideoMenu video(player, &window);
        const auto action = [&window](const char *name) {
            return window.findChild<QAction *>(QString::fromLatin1(name));
        };
        auto *source = action("videoAspectSourceAction");
        auto *wide = action("videoAspect16x9Action");
        auto *stretch = action("videoAspectStretchAction");
        auto *clockwise = action("videoRotateClockwiseAction");
        auto *counterClockwise = action("videoRotateCounterClockwiseAction");
        auto *reset = action("videoRotationResetAction");
        QVERIFY(source && wide && stretch && clockwise && counterClockwise && reset);
        QVERIFY(source->isChecked() && !reset->isEnabled());
        wide->trigger();
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Ratio16x9);
        QVERIFY(wide->isChecked() && !source->isChecked());
        stretch->trigger();
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Stretch);
        QVERIFY(stretch->isChecked() && !wide->isChecked());
        player.setVideoAspect(VideoAspect::Source);
        QVERIFY(source->isChecked() && !stretch->isChecked());
        counterClockwise->trigger();
        clockwise->trigger();
        clockwise->trigger();
        QCOMPARE(player.videoPresentation().rotation, 90);
        QVERIFY(reset->isEnabled());
        QVERIFY(video.menu()->title().contains(QStringLiteral("90")));
        reset->trigger();
        QCOMPARE(player.videoPresentation().rotation, 0);
        QVERIFY(!reset->isEnabled());
        QVERIFY(window.actions().contains(clockwise) && window.actions().contains(source));
        QCOMPARE(clockwise->shortcut(), QKeySequence(Qt::ALT | Qt::Key_Right));
        QCOMPARE(counterClockwise->shortcut(), QKeySequence(Qt::ALT | Qt::Key_Left));
        QVERIFY(!player.state().active);
    }

    void mainWindowReusesMenuInMenuBarAndContextMenu() {
        MainWindow window(true);
        auto *video = window.findChild<QMenu *>(QStringLiteral("videoMenu"));
        QVERIFY(video);
        QVERIFY(window.menuBar()->actions().contains(video->menuAction()));
        bool foundInContextMenu = false;
        for (auto *menu : window.findChildren<QMenu *>()) {
            if (menu != video && menu->actions().contains(video->menuAction()))
                foundInContextMenu = true;
        }
        QVERIFY(foundInContextMenu);
    }
};

QTEST_MAIN(VideoWindowTest)
#include "video_window_test.moc"
