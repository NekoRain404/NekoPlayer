#include "playback/mpvcontroller.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

namespace {
QString option(MpvController &player, const char *name) {
    char *value = mpv_get_property_string(player.handle(), name);
    const auto result = QString::fromUtf8(value);
    mpv_free(value);
    return result;
}
double point(MpvController &player, const char *name) {
    double value = -1;
    return mpv_get_property(player.handle(), name, MPV_FORMAT_DOUBLE, &value) >= 0 ? value : -1;
}
} // namespace

class AbLoopTest final : public QObject {
    Q_OBJECT
  private slots:
    void recoveryCleanupCannotClearReentrantReplacementLoop() {
        MpvController player(true, true);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        player.openFile(path);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(2.0));
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
        player.prepareForRendererReset();
        QTest::qWait(50);
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        bool replaced = false;
        connect(&player, &MpvController::errorOccurred, this, [&] {
            if (replaced)
                return;
            replaced = true;
            QCoreApplication::processEvents();
            QSignalSpy loaded(&player, &MpvController::fileLoaded);
            player.openFile(path);
            QTRY_COMPARE(loaded.count(), 1);
            QTRY_VERIFY(player.state().active && player.state().seekable);
            player.setPaused(true);
            QTRY_VERIFY(player.state().paused);
            QVERIFY(player.setLoopStart(0.6));
            QVERIFY(player.setLoopEnd(2.5));
        });
        player.resumeAfterRendererReset();
        QVERIFY(replaced);
        QCOMPARE(player.abLoop().start.value(), 0.6);
        QCOMPARE(player.abLoop().end.value(), 2.5);
        QTRY_COMPARE(point(player, "ab-loop-a"), 0.6);
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.5);
    }

    void clearingIntervalPreservesWholeFileRepeat() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&player, &MpvController::playbackFinished);
        player.setFileLoopEnabled(true);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(1.2));
        QTRY_COMPARE(point(player, "ab-loop-b"), 1.2);
        QVERIFY(player.clearAbLoop());
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
        QCOMPARE(option(player, "loop-file"), QStringLiteral("inf"));
        player.seekAbsolute(3.8);
        QTRY_VERIFY(qAbs(player.state().position - 3.8) < 0.08);
        player.setPaused(false);
        QTRY_VERIFY(player.state().position < 1);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(finished.isEmpty());
    }

    void rejectedRendererRecoveryClearsLoop() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(2.0));
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
        player.prepareForRendererReset();
        QTest::qWait(50);
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        player.resumeAfterRendererReset();
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        // Rejected clears can be repaired once reply space is available.
        QCoreApplication::processEvents();
        QVERIFY(player.clearAbLoop());
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
    }

    void failedRendererRecoveryClearsLoop() {
        QTemporaryDir media;
        QVERIFY(media.isValid());
        const auto path = media.filePath(QStringLiteral("recovery.mkv"));
        QVERIFY(QFile::copy(QString::fromUtf8(TEST_MEDIA_PATH), path));
        MpvController player(true, true);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        player.openFile(path);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(2.0));
        player.prepareForRendererReset();
        QVERIFY(QFile::remove(path));
        player.resumeAfterRendererReset();
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QVERIFY(!player.setLoopStart(0));
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
    }

    void partiallyRejectedClearCanBeRetried() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(2.0));
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
        QTest::qWait(50);
        int status = 0;
        for (int count = 0; count < 4096 && status >= 0; ++count)
            status = mpv_get_property_async(player.handle(), 0, "mpv-version", MPV_FORMAT_STRING);
        QCOMPARE(status, MPV_ERROR_EVENT_QUEUE_FULL);
        QVERIFY(!player.setLoopStart(0.6));
        QCOMPARE(player.abLoop().start.value(), 0.4);
        QVERIFY(mpv_wait_event(player.handle(), 1)->event_id != MPV_EVENT_NONE);
        QVERIFY(!player.clearAbLoop());
        QVERIFY(!player.abLoop().start);
        QCOMPARE(player.abLoop().end.value(), 2.0);
        QVERIFY(!errors.isEmpty());
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QCoreApplication::processEvents();
        QVERIFY(player.clearAbLoop());
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
    }

    void endpointsReachBackendWithoutReloadOrUnpausing() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy changed(&player, &MpvController::abLoopChanged);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.5));
        QVERIFY(player.setLoopEnd(2.0));
        QCOMPARE(player.abLoop().start.value(), 0.5);
        QCOMPARE(player.abLoop().end.value(), 2.0);
        QTRY_COMPARE(point(player, "ab-loop-a"), 0.5);
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
        QVERIFY(player.state().paused);
        QCOMPARE(loaded.count(), 1);
        QCOMPARE(changed.count(), 2);
        QVERIFY(player.clearAbLoop());
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
    }

    void nativeLoopActuallySeeksBackWithoutFinishingFile() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy finished(&player, &MpvController::playbackFinished);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(1.2));
        player.seekAbsolute(1.0);
        QTRY_VERIFY(qAbs(player.state().position - 1.0) < 0.08);
        player.setPaused(false);
        QTRY_VERIFY(player.state().position < 0.8);
        QVERIFY(player.state().active);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(finished.isEmpty());
        QVERIFY(player.clearAbLoop());
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
        player.seekAbsolute(3.8);
        QTRY_COMPARE(finished.count(), 1);
    }

    void invalidEndpointsCannotEnableAnInvalidLoop() {
        MpvController player(true, true);
        QVERIFY(!player.setLoopStart(0));
        QVERIFY(!player.setLoopEnd(1));
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(!player.setLoopEnd(1));
        for (const auto value : {-1.0, 5.0, std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN()}) {
            QVERIFY(!player.setLoopStart(value));
            QVERIFY(!player.setLoopEnd(value));
        }
        QVERIFY(player.setLoopStart(0));
        QVERIFY(!player.setLoopEnd(0));
        QVERIFY(player.setLoopEnd(2));
        QVERIFY(!player.setLoopStart(2));
        QVERIFY(!player.setLoopStart(3));
        QCOMPARE(player.abLoop().start.value(), 0.0);
        QCOMPARE(player.abLoop().end.value(), 2.0);
        QTRY_COMPARE(point(player, "ab-loop-a"), 0.0);
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
    }

    void stopReplacementAndFailureClearMediaLocalPoints() {
        MpvController player(true, true);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        player.openFile(path);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QVERIFY(player.setLoopStart(0.2));
        QVERIFY(player.setLoopEnd(1.0));
        player.stop();
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QVERIFY(!player.setLoopStart(0));
        QTRY_VERIFY(!player.state().active);
        player.openFile(path);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().seekable);
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QVERIFY(player.setLoopStart(0.3));
        QVERIFY(player.setLoopEnd(1.5));
        player.openFile(path);
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QVERIFY(!player.setLoopEnd(1));
        QTRY_COMPARE(loaded.count(), 3);
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
        QTRY_VERIFY(player.state().seekable);
        QVERIFY(player.setLoopStart(0.4));
        player.openFile(QStringLiteral("/not/a/neko-media.mkv"));
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
    }

    void rendererRecoveryRetainsCurrentMediaLoop() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(player.setLoopStart(0.4));
        QVERIFY(player.setLoopEnd(2.0));
        player.seekAbsolute(1);
        QTRY_VERIFY(qAbs(player.state().position - 1) < 0.08);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().seekable && player.state().paused);
        QCOMPARE(player.abLoop().start.value(), 0.4);
        QCOMPARE(player.abLoop().end.value(), 2.0);
        QTRY_COMPARE(point(player, "ab-loop-a"), 0.4);
        QTRY_COMPARE(point(player, "ab-loop-b"), 2.0);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 3);
        QVERIFY(!player.abLoop().start && !player.abLoop().end);
        QTRY_COMPARE(option(player, "ab-loop-a"), QStringLiteral("no"));
        QTRY_COMPARE(option(player, "ab-loop-b"), QStringLiteral("no"));
    }
};

QTEST_GUILESS_MAIN(AbLoopTest)
#include "ab_loop_test.moc"
