#include "playback/mpv/mpvcontroller.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class RecordingTest final : public QObject {
    Q_OBJECT
  private slots:
    void recordsTheCurrentStreamToDisk() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MpvController player(true, true);
        QSignalSpy changed(&player, &MpvController::recordingChanged);
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QTRY_VERIFY(player.state().active && player.state().seekable);
        const auto path = directory.filePath(QStringLiteral("capture.mkv"));
        QVERIFY(player.startRecording(path));
        QVERIFY(player.isRecording());
        QCOMPARE(player.recordingPath(), path);
        QCOMPARE(changed.count(), 1);
        QTRY_VERIFY(QFileInfo(path).size() > 0);
        player.stopRecording();
        QVERIFY(!player.isRecording());
        QVERIFY(player.recordingPath().isEmpty());
        QCOMPARE(changed.count(), 2);
        player.stop();
    }

    void rejectsRecordingWithoutMedia() {
        MpvController player(true, true);
        QVERIFY(!player.startRecording(QStringLiteral("/tmp/nekoplayer-recording-test.mkv")));
        QVERIFY(!player.isRecording());
    }
};

QTEST_GUILESS_MAIN(RecordingTest)
#include "recording_test.moc"
