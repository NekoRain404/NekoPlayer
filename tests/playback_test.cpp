#include "playback/mpvcontroller.h"
#include <QSignalSpy>
#include <QtTest>
#include <cmath>
#include <memory>

class PlaybackTest final : public QObject {
    Q_OBJECT
  private slots:
    void initializes() {
        const MpvController player(true, true);
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        QVERIFY(player.handle());
    }

    void requestsSafeHardwareDecode() {
        const MpvController player(true, true);
        const auto hardwareDecode = std::unique_ptr<char, decltype(&mpv_free)>(
            mpv_get_property_string(player.handle(), "hwdec"), &mpv_free);
        QVERIFY(hardwareDecode);
        QCOMPARE(QByteArray(hardwareDecode.get()), QByteArray("auto-safe"));
    }

    void exposesAutomaticAudioDevice() {
        MpvController player(true, true);
        QTRY_VERIFY_WITH_TIMEOUT(!player.audioDevices().isEmpty(), 5000);
        QCOMPARE(player.audioDevices().constFirst().name, QStringLiteral("auto"));
        QTRY_COMPARE_WITH_TIMEOUT(player.selectedAudioDevice(), QStringLiteral("auto"), 5000);
    }

    void rejectsUnavailableAudioDeviceWithoutChangingSelection() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QTRY_VERIFY_WITH_TIMEOUT(!player.audioDevices().isEmpty(), 5000);

        player.setAudioDevice(QStringLiteral("neko-test/unavailable"));

        QCOMPARE(errors.count(), 1);
        QCOMPARE(player.selectedAudioDevice(), QStringLiteral("auto"));
    }

    void transport() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_VERIFY(player.state().duration > 3.5 && player.state().duration < 4.5);
        QCOMPARE(player.state().path, QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().position > 0.15);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTest::qWait(100);
        const auto pausedAt = player.state().position;
        QTest::qWait(200);
        QVERIFY(std::abs(player.state().position - pausedAt) < 0.08);
        player.seekAbsolute(2.0);
        QTRY_VERIFY(std::abs(player.state().position - 2.0) < 0.25);
        player.seekRelative(-1.0);
        QTRY_VERIFY(std::abs(player.state().position - 1.0) < 0.25);
        player.togglePause();
        QTRY_VERIFY(!player.state().paused);
        QTRY_VERIFY(player.state().position > 1.2);
        player.stop();
        QTRY_VERIFY(!player.state().active);
        QTRY_COMPARE(player.state().duration, 0.0);
        QTRY_COMPARE(player.state().position, 0.0);
        QVERIFY(!player.state().seekable);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(!player.state().paused);
    }

    void stopCancelsRendererRecovery() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(loaded.count(), 1);
        QTRY_VERIFY(player.state().active);
        player.prepareForRendererReset();
        player.stop();
        QTRY_VERIFY(!player.state().active);
        player.resumeAfterRendererReset();
        QTest::qWait(300);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(!player.state().active);
    }

    void volumeAndMute() {
        MpvController player(true, true);
        player.setVolume(33);
        QTRY_COMPARE(player.state().volume, 33.0);
        player.setMuted(true);
        QTRY_VERIFY(player.state().muted);
        player.setMuted(false);
        QTRY_VERIFY(!player.state().muted);
        player.setVolume(200);
        QTRY_COMPARE(player.state().volume, 100.0);
        player.setVolume(-20);
        QTRY_COMPARE(player.state().volume, 0.0);
    }

    void failureRecovery() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QStringLiteral("/nonexistent-neko-test/file.mkv"));
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);
        QVERIFY(!player.state().active);
        QCOMPARE(player.state().position, 0.0);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 5000);
        player.openFile(QStringLiteral("/nonexistent-neko-test/second.mkv"));
        QTRY_VERIFY(errors.count() >= 2);
        QTRY_VERIFY(!player.state().active);
        QTRY_COMPARE(player.state().duration, 0.0);
    }

    void eof() {
        MpvController player(true, true);
        QSignalSpy ended(&player, &MpvController::playbackFinished);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        player.seekAbsolute(3.5);
        QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
        QTRY_VERIFY(!player.state().active);
        QVERIFY(!player.state().path.isEmpty());
    }

    void seekBeyondKnownDurationStaysBounded() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 5000);
        QTRY_VERIFY(player.state().active && player.state().seekable);
        QTRY_VERIFY(player.state().duration > 3.5);
        const auto duration = player.state().duration;
        player.setPaused(true);
        player.seekAbsolute(1000.0);
        QTRY_VERIFY_WITH_TIMEOUT(
            !player.state().active || player.state().position <= duration + 0.05, 5000);
        player.seekAbsolute(-5.0);
        QTRY_VERIFY_WITH_TIMEOUT(!player.state().active || player.state().position < 0.05, 5000);
    }

    void rapidSwitchAndClose() {
        for (int i = 0; i < 3; ++i) {
            MpvController player(true, true);
            player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
            player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
            QTRY_VERIFY(player.state().active);
        }
    }
};

QTEST_GUILESS_MAIN(PlaybackTest)
#include "playback_test.moc"
