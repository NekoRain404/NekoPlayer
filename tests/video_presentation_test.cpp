#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QSignalSpy>
#include <QtTest>
#include <climits>
#include <cmath>
#include <limits>

namespace {
QString option(MpvController &player, const char *name) {
    char *value = mpv_get_property_string(player.handle(), name);
    const auto result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}

double aspectOverride(MpvController &player) {
    double result = std::numeric_limits<double>::quiet_NaN();
    if (mpv_get_property(player.handle(), "video-aspect-override", MPV_FORMAT_DOUBLE, &result) < 0)
        return std::numeric_limits<double>::quiet_NaN();
    return result;
}
} // namespace

class VideoPresentationTest final : public QObject {
    Q_OBJECT
  private slots:
    void reselectingPriorAspectRepairsPartialSubmission() {
        MpvController player(true, true);
        QVERIFY(player.isReady());
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        // Settle startup events before filling the real libmpv reply queue.
        QTest::qWait(50);
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        // Release exactly one reply slot: only the first aspect setter fits.
        QVERIFY(mpv_wait_event(player.handle(), 1)->event_id != MPV_EVENT_NONE);
        player.setVideoAspect(VideoAspect::Ratio4x3);
        QVERIFY(!errors.isEmpty());
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Source);
        QTRY_VERIFY(std::abs(aspectOverride(player) - 1.333333) < 0.00001);
        // A user can recover by reselecting the still-checked prior choice.
        QCoreApplication::processEvents();
        player.setVideoAspect(VideoAspect::Source);
        QTRY_COMPARE(aspectOverride(player), -2.0);
        QTRY_COMPARE(option(player, "keepaspect"), QStringLiteral("yes"));
    }

    void aspectRequestsReachBackend_data() {
        QTest::addColumn<VideoAspect>("aspect");
        QTest::addColumn<double>("ratio");
        QTest::newRow("4:3") << VideoAspect::Ratio4x3 << 1.333333;
        QTest::newRow("16:9") << VideoAspect::Ratio16x9 << 1.777778;
        QTest::newRow("16:10") << VideoAspect::Ratio16x10 << 1.6;
        QTest::newRow("21:9") << VideoAspect::Ratio21x9 << 2.333333;
    }

    void aspectRequestsReachBackend() {
        QFETCH(VideoAspect, aspect);
        QFETCH(double, ratio);
        MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setVideoAspect(VideoAspect::Stretch);
        QTRY_COMPARE(option(player, "keepaspect"), QStringLiteral("no"));
        player.setVideoAspect(aspect);
        QCOMPARE(player.videoPresentation().aspect, aspect);
        QTRY_VERIFY(std::abs(aspectOverride(player) - ratio) < 0.00001);
        QTRY_COMPARE(option(player, "keepaspect"), QStringLiteral("yes"));
        player.setVideoAspect(VideoAspect::Source);
        // mpv 0.41 accepts "no", but returns its numeric sentinel through the API.
        QTRY_COMPARE(aspectOverride(player), -2.0);
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Source);
        QVERIFY(errors.isEmpty());
    }

    void rapidMixedRotationsAccumulateBeforeEvents() {
        MpvController player(true, true);
        QSignalSpy changed(&player, &MpvController::videoPresentationChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.rotateVideoCounterClockwise();
        QCOMPARE(player.videoPresentation().rotation, 270);
        player.rotateVideoClockwise();
        player.rotateVideoClockwise();
        player.rotateVideoClockwise();
        player.rotateVideoClockwise();
        QCOMPARE(player.videoPresentation().rotation, 270);
        QTRY_COMPARE(option(player, "video-rotate"), QStringLiteral("270"));
        QCOMPARE(changed.count(), 5);
        player.setVideoRotation(0);
        QTRY_COMPARE(option(player, "video-rotate"), QStringLiteral("0"));
        QVERIFY(errors.isEmpty());
    }

    void normalizesQuarterTurnsAndRejectsUnsupportedValues() {
        MpvController player(true, true);
        player.setVideoRotation(-450);
        QCOMPARE(player.videoPresentation().rotation, 270);
        QTRY_COMPARE(option(player, "video-rotate"), QStringLiteral("270"));
        player.setVideoRotation(810);
        QCOMPARE(player.videoPresentation().rotation, 90);
        QTRY_COMPARE(option(player, "video-rotate"), QStringLiteral("90"));
        QSignalSpy changed(&player, &MpvController::videoPresentationChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setVideoRotation(90);
        for (const int value : {1, -1, 45, 359, INT_MIN, INT_MAX})
            player.setVideoRotation(value);
        player.setVideoAspect(static_cast<VideoAspect>(-1));
        player.setVideoAspect(static_cast<VideoAspect>(999));
        QCOMPARE(player.videoPresentation().rotation, 90);
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Source);
        // A valid command behind invalid requests is a backend queue barrier.
        player.setVideoAspect(VideoAspect::Ratio4x3);
        QTRY_VERIFY(std::abs(aspectOverride(player) - 1.333333) < 0.00001);
        QCOMPARE(option(player, "video-rotate"), QStringLiteral("90"));
        QCOMPARE(changed.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void changesPreservePausedMediaTracksAndQueue() {
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
        player.setVideoAspect(VideoAspect::Ratio4x3);
        player.rotateVideoClockwise();
        player.setVideoAspect(VideoAspect::Stretch);
        QTRY_COMPARE(option(player, "video-rotate"), QStringLiteral("90"));
        QTRY_COMPARE(option(player, "keepaspect"), QStringLiteral("no"));
        QCOMPARE(aspectOverride(player), -2.0);
        QVERIFY(player.state().active && player.state().paused);
        QVERIFY(std::abs(player.state().position - 1) < 0.08);
        QCOMPARE(player.tracks(), tracks);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(playlist.model()->currentIndex(), 0);
        QCOMPARE(playlist.model()->rowCount(), 2);
        QCOMPARE(option(player, "hwdec"), QStringLiteral("auto-safe"));
        player.setVideoAspect(VideoAspect::Source);
        QTRY_COMPARE(option(player, "keepaspect"), QStringLiteral("yes"));
        QCOMPARE(aspectOverride(player), -2.0);
        QVERIFY(errors.isEmpty());
    }

    void retainsSessionIntentAcrossReloadStopAndRendererRecovery() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.setVideoAspect(VideoAspect::Ratio16x10);
        player.setVideoRotation(180);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(player.state().active);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active && player.state().paused);
        QCOMPARE(option(player, "video-rotate"), QStringLiteral("180"));
        QVERIFY(std::abs(aspectOverride(player) - 1.6) < 0.00001);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        player.openFile(QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 2);
        QCOMPARE(option(player, "video-rotate"), QStringLiteral("180"));
        QCOMPARE(player.videoPresentation().aspect, VideoAspect::Ratio16x10);
        QVERIFY(std::abs(aspectOverride(player) - 1.6) < 0.00001);
        QVERIFY(errors.isEmpty());
        MpvController fresh(true, true);
        QCOMPARE(option(fresh, "video-rotate"), QStringLiteral("0"));
        QCOMPARE(aspectOverride(fresh), -2.0);
        QCOMPARE(option(fresh, "keepaspect"), QStringLiteral("yes"));
    }
};

QTEST_GUILESS_MAIN(VideoPresentationTest)
#include "video_presentation_test.moc"
