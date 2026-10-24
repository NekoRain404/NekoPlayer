#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QSignalSpy>
#include <QtTest>
#include <cmath>
#include <limits>

namespace {
double nativeValue(MpvController &player, const char *name) {
    double value = std::numeric_limits<double>::quiet_NaN();
    if (mpv_get_property(player.handle(), name, MPV_FORMAT_DOUBLE, &value) < 0)
        return std::numeric_limits<double>::quiet_NaN();
    return value;
}

QString nativeText(MpvController &player, const char *name) {
    char *value = mpv_get_property_string(player.handle(), name);
    const auto result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}

} // namespace

class VideoFramingTest final : public QObject {
    Q_OBJECT
  private slots:
    void settersReachNativeOptionsAndClampAtApplicationLimits() {
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QCOMPARE(player.videoFraming(), VideoFraming{});
        QCOMPARE(nativeValue(player, "video-zoom"), 0.0);
        QCOMPARE(nativeValue(player, "video-pan-x"), 0.0);
        QCOMPARE(nativeValue(player, "video-pan-y"), 0.0);

        player.setVideoZoom(0.75);
        player.setVideoPanX(-0.25);
        player.setVideoPanY(0.5);
        QCOMPARE(player.videoFraming(), (VideoFraming{0.75, -0.25, 0.5}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 0.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.5);

        player.setVideoZoom(100);
        player.setVideoPanX(-100);
        player.setVideoPanY(100);
        QCOMPARE(player.videoFraming(), (VideoFraming{3, -1, 1}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 3.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -1.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 1.0);

        player.setVideoZoom(-100);
        player.setVideoPanX(100);
        player.setVideoPanY(-100);
        QCOMPARE(player.videoFraming(), (VideoFraming{-2, 1, -1}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), -2.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 1.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -1.0);
        QVERIFY(errors.isEmpty());
    }

    void rejectsNonFiniteSettersAndDeltasWithoutChangingIntent() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        player.setVideoZoom(0.75);
        player.setVideoPanX(-0.25);
        player.setVideoPanY(0.5);
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 0.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.5);
        QSignalSpy changed(&player, &MpvController::videoFramingChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        for (const double value :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
              -std::numeric_limits<double>::infinity()}) {
            player.setVideoZoom(value);
            player.setVideoPanX(value);
            player.setVideoPanY(value);
            player.adjustVideoZoom(value);
            player.adjustVideoPanX(value);
            player.adjustVideoPanY(value);
            QCOMPARE(player.videoFraming(), (VideoFraming{0.75, -0.25, 0.5}));
        }
        QCOMPARE(changed.count(), 0);
        // A distinct accepted command behind rejected requests flushes their queue.
        player.setVideoZoom(1.25);
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 1.25);
        QCOMPARE(nativeValue(player, "video-pan-x"), -0.25);
        QCOMPARE(nativeValue(player, "video-pan-y"), 0.5);
        QCOMPARE(changed.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void rapidMixedDeltasAccumulateFromLatestAcceptedIntent() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        // Do not dispatch Qt events between requests: observed backend values lag.
        player.adjustVideoZoom(0.5);
        player.adjustVideoPanX(0.25);
        player.adjustVideoPanY(-0.5);
        player.adjustVideoZoom(0.25);
        player.adjustVideoPanY(0.125);
        player.adjustVideoPanX(-0.5);
        QCOMPARE(player.videoFraming(), (VideoFraming{0.75, -0.25, -0.375}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 0.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -0.375);

        // Overshooting a limit must not build up hidden movement beyond it.
        player.adjustVideoZoom(100);
        player.adjustVideoZoom(-0.25);
        player.adjustVideoPanX(-100);
        player.adjustVideoPanX(0.125);
        player.adjustVideoPanY(100);
        player.adjustVideoPanY(-0.25);
        QCOMPARE(player.videoFraming(), (VideoFraming{2.75, -0.875, 0.75}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 2.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.875);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.75);

        player.adjustVideoZoom(-100);
        player.adjustVideoZoom(0.125);
        player.adjustVideoPanX(100);
        player.adjustVideoPanX(-0.25);
        player.adjustVideoPanY(-100);
        player.adjustVideoPanY(0.125);
        QCOMPARE(player.videoFraming(), (VideoFraming{-1.875, 0.75, -0.875}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), -1.875);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 0.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -0.875);
        QVERIFY(errors.isEmpty());
    }

    void resetRestoresAllNativeValuesAndRepeatedResetIsUnchanged() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        player.setVideoZoom(2);
        player.setVideoPanX(-0.5);
        player.setVideoPanY(0.75);
        player.resetVideoFraming();
        QCOMPARE(player.videoFraming(), VideoFraming{});
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 0.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 0.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.0);
        QSignalSpy changed(&player, &MpvController::videoFramingChanged);
        player.resetVideoFraming();
        player.setVideoZoom(0);
        player.setVideoPanX(0);
        player.setVideoPanY(0);
        QCOMPARE(changed.count(), 0);
    }

    void rejectedSubmissionDoesNotAcceptIntentAndCanBeRetried() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        QSignalSpy changed(&player, &MpvController::videoFramingChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QCOMPARE(mpv_request_event(player.handle(), MPV_EVENT_IDLE, 0), 0);
        while (mpv_wait_event(player.handle(), 0)->event_id != MPV_EVENT_NONE) {
        }
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        player.setVideoZoom(1);
        player.setVideoPanX(0.5);
        player.setVideoPanY(-0.5);
        QCOMPARE(player.videoFraming(), VideoFraming{});
        QCOMPARE(changed.count(), 0);
        QVERIFY(!errors.isEmpty());

        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     player.setVideoZoom(1);
                                     return player.videoFraming().zoom == 1;
                                 })(),
                                 2000);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     player.setVideoPanX(0.5);
                                     return player.videoFraming().panX == 0.5;
                                 })(),
                                 2000);
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     player.setVideoPanY(-0.5);
                                     return player.videoFraming().panY == -0.5;
                                 })(),
                                 2000);
        QCOMPARE(player.videoFraming(), (VideoFraming{1, 0.5, -0.5}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 1.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 0.5);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -0.5);
    }

    void resetFailureDoesNotOverwriteFramingChosenByReentrantErrorHandler() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        player.setVideoZoom(2);
        player.setVideoPanX(-0.75);
        player.setVideoPanY(0.75);
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 2.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.75);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.75);
        QCOMPARE(mpv_request_event(player.handle(), MPV_EVENT_IDLE, 0), 0);
        while (mpv_wait_event(player.handle(), 0)->event_id != MPV_EVENT_NONE) {
        }
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);

        bool errorHandled = false;
        connect(&player, &MpvController::errorOccurred, &player, [&] {
            if (errorHandled)
                return;
            errorHandled = true;
            // Free reply space before accepting a new choice inside the reset call.
            QCoreApplication::processEvents();
            player.setVideoZoom(1);
            player.setVideoPanX(0.25);
            player.setVideoPanY(-0.5);
        });
        player.resetVideoFraming();
        QVERIFY(errorHandled);
        // A reset must finish its old submissions before reporting their errors.
        QCOMPARE(player.videoFraming(), (VideoFraming{1, 0.25, -0.5}));
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 1.0);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 0.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -0.5);
    }

    void changesPreservePausedPositionTracksQueueAndDecoderPreference() {
        MpvController player(true, true);
        PlaylistController playlist(player);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        playlist.addFiles(
            {QString::fromUtf8(TEST_TRACK_MEDIA_PATH), QString::fromUtf8(TEST_TRACK_MEDIA_PATH)});
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.0);
        QTRY_VERIFY(std::abs(player.state().position - 1) < 0.08);
        player.addSubtitle(QString::fromUtf8(TEST_EXTERNAL_SUBTITLE_PATH));
        QTRY_COMPARE(player.tracks().size(), 4);
        QTRY_VERIFY(player.tracks().back().external && player.tracks().back().selected);
        const auto tracks = player.tracks();
        const auto path = player.state().path;
        player.setVideoZoom(1.25);
        player.setVideoPanX(-0.375);
        player.setVideoPanY(0.625);
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 1.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), -0.375);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), 0.625);
        QVERIFY(player.state().active && player.state().paused);
        QVERIFY(std::abs(player.state().position - 1) < 0.08);
        QCOMPARE(player.state().path, path);
        QCOMPARE(player.tracks(), tracks);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(playlist.model()->currentIndex(), 0);
        QCOMPARE(playlist.model()->rowCount(), 2);
        QVERIFY(player.hardwareDecodingEnabled());
        QCOMPARE(nativeText(player, "hwdec"), QStringLiteral("auto-safe"));
        QVERIFY(errors.isEmpty());
    }

    void framingSurvivesRendererRecoveryStopAndReopenButNewControllerStartsNeutral() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setVideoZoom(0.5);
        player.setVideoPanX(0.25);
        player.setVideoPanY(-0.75);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTRY_COMPARE(nativeValue(player, "video-zoom"), 0.5);
        QTRY_COMPARE(nativeValue(player, "video-pan-x"), 0.25);
        QTRY_COMPARE(nativeValue(player, "video-pan-y"), -0.75);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QCOMPARE(player.videoFraming(), (VideoFraming{0.5, 0.25, -0.75}));
        QCOMPARE(nativeValue(player, "video-zoom"), 0.5);
        QCOMPARE(nativeValue(player, "video-pan-x"), 0.25);
        QCOMPARE(nativeValue(player, "video-pan-y"), -0.75);

        player.stop();
        QTRY_VERIFY(!player.state().active);
        QCOMPARE(player.videoFraming(), (VideoFraming{0.5, 0.25, -0.75}));
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active);
        QCOMPARE(player.videoFraming(), (VideoFraming{0.5, 0.25, -0.75}));
        QCOMPARE(nativeValue(player, "video-zoom"), 0.5);
        QCOMPARE(nativeValue(player, "video-pan-x"), 0.25);
        QCOMPARE(nativeValue(player, "video-pan-y"), -0.75);
        QVERIFY(errors.isEmpty());

        MpvController fresh(true, true);
        QVERIFY(fresh.isReady());
        QCOMPARE(fresh.videoFraming(), VideoFraming{});
        QCOMPARE(nativeValue(fresh, "video-zoom"), 0.0);
        QCOMPARE(nativeValue(fresh, "video-pan-x"), 0.0);
        QCOMPARE(nativeValue(fresh, "video-pan-y"), 0.0);
    }
};

QTEST_GUILESS_MAIN(VideoFramingTest)
#include "video_framing_test.moc"
