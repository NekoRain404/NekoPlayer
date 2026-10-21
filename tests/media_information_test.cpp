#include "playback/mpvcontroller.h"

#include <QFileInfo>
#include <QSignalSpy>
#include <QtTest>

class MediaInformationTest final : public QObject {
    Q_OBJECT
  private slots:
    void generatedStreamKeepsUnavailableSizeUnknown() {
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        player.openFile(QStringLiteral("av://lavfi:testsrc2=size=64x64:rate=25"));
        QTRY_COMPARE(player.mediaInformation().videoWidth, 64);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        QTest::qWait(100);
        QCOMPARE(player.mediaInformation().fileSize, -1);
        QCOMPARE(player.mediaInformation().audioSampleRate, 0);
        QVERIFY(player.mediaInformation().audioCodec.isEmpty());
        QVERIFY(errors.isEmpty());
    }

    void reportsActualDecodedMediaWithoutDisturbingPlayback() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        QSignalSpy information(&player, &MpvController::mediaInformationChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        player.openFile(path);
        QTRY_COMPARE(player.mediaInformation().videoWidth, 320);
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        const auto &info = player.mediaInformation();
        QCOMPARE(info.videoHeight, 180);
        QCOMPARE(info.videoCodec, QStringLiteral("mpeg4"));
        QCOMPARE(info.videoPixelFormat, QStringLiteral("yuv420p"));
        QCOMPARE(info.audioCodec, QStringLiteral("pcm_s16le"));
        QVERIFY(!info.audioChannels.isEmpty() && !info.audioSampleFormat.isEmpty());
        QVERIFY(info.fileFormat == QStringLiteral("mkv") ||
                info.fileFormat.contains(QStringLiteral("matroska")));
        QCOMPARE(info.fileSize, QFileInfo(path).size());
        QTRY_COMPARE(info.containerFrameRate, 25.0);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.seekAbsolute(1.2);
        QTRY_VERIFY(qAbs(player.state().position - 1.2) < 0.005);
        QTest::qWait(100);
        const auto settled = info;
        const auto count = information.count();
        player.setPaused(false);
        QTRY_VERIFY(player.state().position > 1.4);
        QCOMPARE(info, settled);
        QCOMPARE(information.count(), count);
        QCOMPARE(loaded.count(), 1);
        QVERIFY(errors.isEmpty());
    }

    void replacementStopAndFailureClearPreviousInformation() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(player.mediaInformation().videoWidth, 320);
        player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
        QCOMPARE(player.mediaInformation(), MediaInformation{});
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        QCOMPARE(player.mediaInformation().videoWidth, 0);
        QVERIFY(player.mediaInformation().videoCodec.isEmpty());
        QVERIFY(player.mediaInformation().videoPixelFormat.isEmpty());
        player.stop();
        QCOMPARE(player.mediaInformation(), MediaInformation{});
        QTRY_VERIFY(!player.state().active);
        QTest::qWait(100);
        QCOMPARE(player.mediaInformation(), MediaInformation{});
        QSignalSpy failed(&player, &MpvController::mediaLoadFailed);
        player.openFile(QStringLiteral("/missing-neko-info.mkv"));
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(player.mediaInformation(), MediaInformation{});
    }

    void identicalReloadAndRendererRecoveryRepopulateInformation() {
        MpvController player(true, true);
        QSignalSpy loaded(&player, &MpvController::fileLoaded);
        const auto path = QString::fromUtf8(TEST_MEDIA_PATH);
        player.openFile(path);
        QTRY_COMPARE(player.mediaInformation().videoWidth, 320);
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        QTRY_VERIFY(!player.mediaInformation().audioSampleFormat.isEmpty());
        const auto expected = player.mediaInformation();
        player.openFile(path);
        QThread::msleep(300);
        QTRY_COMPARE(loaded.count(), 2);
        QTRY_COMPARE(player.mediaInformation().videoWidth, 320);
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        QTRY_COMPARE(player.mediaInformation(), expected);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.prepareForRendererReset();
        player.resumeAfterRendererReset();
        QTRY_COMPARE(loaded.count(), 3);
        QTRY_COMPARE(player.mediaInformation().videoWidth, 320);
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        QTRY_COMPARE(player.mediaInformation(), expected);
        QVERIFY(player.state().paused);
    }

    void informationCallbackCanStopOrReplaceWithoutLateRepliesRestoringOldDetails_data() {
        QTest::addColumn<bool>("replace");
        QTest::newRow("stop") << false;
        QTest::newRow("replace") << true;
    }

    void informationCallbackCanStopOrReplaceWithoutLateRepliesRestoringOldDetails() {
        QFETCH(bool, replace);
        MpvController player(true, true);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        bool changed = false;
        connect(&player, &MpvController::mediaInformationChanged, &player, [&] {
            if (changed || player.mediaInformation() == MediaInformation{})
                return;
            changed = true;
            if (replace)
                player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
            else
                player.stop();
        });
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(changed);
        if (replace) {
            QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
            QCOMPARE(player.mediaInformation().videoWidth, 0);
            QVERIFY(player.mediaInformation().videoCodec.isEmpty());
        } else {
            QTRY_VERIFY(!player.state().active);
            QTest::qWait(100);
            QCOMPARE(player.mediaInformation(), MediaInformation{});
        }
        QVERIFY(errors.isEmpty());
    }

    void selectedTrackChangesClearAndRestoreDecoderDetails() {
        MpvController player(true, true);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        qint64 audioId = 0;
        for (const auto &track : player.tracks())
            if (track.type == MediaTrack::Type::Audio && track.selected)
                audioId = track.id;
        QVERIFY(audioId > 0);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
        player.selectAudioTrack(0);
        QTRY_VERIFY(player.mediaInformation().audioCodec.isEmpty());
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 0);
        QVERIFY(player.mediaInformation().audioChannels.isEmpty());
        player.selectAudioTrack(audioId);
        QTRY_COMPARE(player.mediaInformation().audioSampleRate, 48000);
        QTRY_COMPARE(player.mediaInformation().audioCodec, QStringLiteral("pcm_s16le"));
        QVERIFY(player.state().paused);
        const char *disableVideo[]{"set", "vid", "no", nullptr};
        QVERIFY(mpv_command_async(player.handle(), 0, disableVideo) >= 0);
        QTRY_VERIFY(player.mediaInformation().videoCodec.isEmpty());
        QTRY_COMPARE(player.mediaInformation().videoWidth, 0);
        QTRY_VERIFY(player.mediaInformation().videoPixelFormat.isEmpty());
    }
};

QTEST_GUILESS_MAIN(MediaInformationTest)
#include "media_information_test.moc"
