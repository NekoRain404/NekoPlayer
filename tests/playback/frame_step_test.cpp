#include "playback/mpv/mpvcontroller.h"

#include <QSignalSpy>
#include <QtTest>
#include <cmath>

class FrameStepTest final : public QObject {
    Q_OBJECT
  private slots:
    void identicalReloadRefreshesAvailabilityAfterCoalescedEvents() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        player.openFile(path);
        QTRY_VERIFY(player.canStepFrameForward());
        player.openFile(path);
        QThread::msleep(300);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.canStepFrameForward() && player.canStepFrameBackward());
        player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
        QThread::msleep(300);
        QTRY_COMPARE(loaded.count(), 3);
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.openFile(path);
        QThread::msleep(300);
        QTRY_COMPARE(loaded.count(), 4);
        QTRY_VERIFY(player.canStepFrameForward() && player.canStepFrameBackward());
    }

    void stillImagesDoNotOfferFrameStepping() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_STILL_IMAGE_PATH));
        QTRY_COMPARE(loaded.count(), 1);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.stepFrameForward();
        player.stepFrameBackward();
        QTest::qWait(100);
        QVERIFY(player.state().paused && player.state().active);
        QVERIFY(errors.isEmpty());
    }

    void rendererRecoveryAndFailedLoadUpdateEligibility() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.canStepFrameForward());
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.2);
        QTRY_VERIFY(std::abs(player.state().position - 1.2) < 0.005);
        player.prepareForRendererReset();
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.stepFrameForward();
        player.stepFrameBackward();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.canStepFrameForward() && player.canStepFrameBackward());
        QTRY_VERIFY(player.state().paused && std::abs(player.state().position - 1.2) < 0.005);
        player.stepFrameForward();
        QTRY_VERIFY(std::abs(player.state().position - 1.24) < 0.005);
        player.openFile(QStringLiteral("/missing-neko-frame-step.mkv"));
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
    }

    void eligibilityRejectsIdleAudioAndPendingMediaChanges() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.stepFrameForward();
        player.stepFrameBackward();
        player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.stepFrameForward();
        player.stepFrameBackward();
        QTest::qWait(100);
        QVERIFY(player.state().active && !player.state().paused);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        QTRY_VERIFY(player.canStepFrameForward() && player.canStepFrameBackward());
        player.stop();
        QVERIFY(!player.canStepFrameForward() && !player.canStepFrameBackward());
        player.stepFrameForward();
        player.stepFrameBackward();
        QTRY_VERIFY(!player.state().active);
        QVERIFY(errors.isEmpty());
    }

    void steppingPlayingVideoPausesAndCanResume() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.canStepFrameForward() && player.canStepFrameBackward());
        QTRY_VERIFY(player.state().position > 0.3);
        player.stepFrameForward();
        QTRY_VERIFY(player.state().paused);
        const auto forward = player.state().position;
        QTest::qWait(100);
        QVERIFY(std::abs(player.state().position - forward) < 0.005);
        player.setPaused(false);
        QTRY_VERIFY(!player.state().paused && player.state().position > forward + 0.1);
        player.stepFrameBackward();
        QTRY_VERIFY(player.state().paused);
        // Pause is observed before the native backward seek finishes.
        int seeking = 1;
        QTRY_VERIFY(mpv_get_property(player.handle(), "seeking", MPV_FORMAT_FLAG, &seeking) >= 0 &&
                    !seeking);
        double backward = 0;
        QVERIFY(mpv_get_property(player.handle(), "time-pos", MPV_FORMAT_DOUBLE, &backward) >= 0);
        QTRY_VERIFY(std::abs(player.state().position - backward) < 0.005);
        QTest::qWait(100);
        QVERIFY(std::abs(player.state().position - backward) < 0.005);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void pausedVideoStepsByFramesWithoutReloading() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.2);
        QTRY_VERIFY(std::abs(player.state().position - 1.2) < 0.005);
        QVERIFY(QMetaObject::invokeMethod(&player, "stepFrameForward"));
        // The fixture is 25 fps: exactly one frame is 40 ms. Report the observed
        // position when a backend lands on a different boundary so CI failures
        // distinguish a no-op from an off-by-one frame.
        double forwardPosition = 1.2;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     forwardPosition = player.state().position;
                                     return std::abs(forwardPosition - 1.2) > 0.001;
                                 })(),
                                 5000);
        QVERIFY2(
            std::abs(forwardPosition - 1.24) < 0.005,
            qPrintable(
                QStringLiteral("position after frame step: %1").arg(forwardPosition, 0, 'f', 3)));
        QTRY_VERIFY(player.state().paused);
        QTest::qWait(100);
        QVERIFY(std::abs(player.state().position - 1.24) < 0.005);
        QVERIFY(QMetaObject::invokeMethod(&player, "stepFrameBackward"));
        double backwardPosition = forwardPosition;
        QTRY_VERIFY_WITH_TIMEOUT(([&] {
                                     backwardPosition = player.state().position;
                                     return std::abs(backwardPosition - forwardPosition) > 0.001;
                                 })(),
                                 5000);
        QVERIFY2(std::abs(backwardPosition - 1.2) < 0.005,
                 qPrintable(QStringLiteral("position after backward frame step: %1")
                                .arg(backwardPosition, 0, 'f', 3)));
        QVERIFY(player.state().paused);
        player.seekAbsolute(0);
        QTRY_VERIFY(player.state().position < 0.005);
        player.stepFrameBackward();
        QTest::qWait(100);
        QVERIFY(player.state().active && player.state().paused);
        QVERIFY(player.state().position >= 0 && player.state().position < 0.005);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(errors.isEmpty());
    }
};

QTEST_GUILESS_MAIN(FrameStepTest)
#include "frame_step_test.moc"
