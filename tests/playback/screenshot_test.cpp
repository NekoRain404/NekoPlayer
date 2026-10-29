#include "playback/mpv/mpvcontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

class ScreenshotTest final : public QObject {
    Q_OBJECT
  private:
    void loadPausedVideo(MpvController &player,
                         const QString &path = QString::fromUtf8(TEST_MEDIA_PATH)) {
        QVERIFY2(player.isReady(), qPrintable(player.errorString()));
        player.openFile(path);
        QTRY_VERIFY_WITH_TIMEOUT(player.canTakeScreenshot(), 3000);
        // Dimensions can precede the first decoded frame.
        QTRY_VERIFY(player.state().position > 0);
        player.setPaused(true);
        QTRY_VERIFY(player.state().paused);
    }

  private slots:
    void encodesRequestedFormatsToLiteralUnicodePaths_data() {
        QTest::addColumn<QString>("extension");
        QTest::addColumn<QByteArray>("format");
        QTest::newRow("png") << QStringLiteral("png") << QByteArray("png");
        QTest::newRow("jpg") << QStringLiteral("jpg") << QByteArray("jpeg");
        QTest::newRow("jpeg-uppercase") << QStringLiteral("JPEG") << QByteArray("jpeg");
    }

    void encodesRequestedFormatsToLiteralUnicodePaths() {
        QFETCH(QString, extension);
        QFETCH(QByteArray, format);
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path =
            directory.filePath(QStringLiteral("猫 frame ${filename} [1].") + extension);
        MpvController player(true, true);
        loadPausedVideo(player);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        QSignalSpy pending(&player, &MpvController::screenshotPendingChanged);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QVERIFY(player.takeScreenshot(path, false));
        QVERIFY(player.screenshotPending());
        QVERIFY(!player.canTakeScreenshot());
        QCOMPARE(pending.count(), 1);
        QVERIFY(finished.isEmpty());
        const auto duplicate = directory.filePath(QStringLiteral("duplicate.png"));
        QVERIFY(!player.takeScreenshot(duplicate));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toString(), path);
        QVERIFY(finished.at(0).at(1).toBool());
        QVERIFY(!player.screenshotPending());
        QVERIFY(player.canTakeScreenshot());
        QCOMPARE(pending.count(), 2);
        QVERIFY(!QFileInfo::exists(duplicate));
        QVERIFY(errors.isEmpty());
        QImageReader reader(path);
        QCOMPARE(reader.format(), format);
        const auto image = reader.read();
        QVERIFY2(!image.isNull(), qPrintable(reader.errorString()));
        QCOMPARE(image.size(), QSize(320, 180));
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 1);
        QVERIFY(player.state().active && player.state().paused);
    }

    void invalidPathsAreRejectedBeforeSubmission() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MpvController player(true, true);
        loadPausedVideo(player);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        QSignalSpy pending(&player, &MpvController::screenshotPendingChanged);
        for (const auto &path :
             {QString{}, QStringLiteral("relative.png"),
              directory.filePath(QStringLiteral("unsupported.webp")),
              directory.filePath(QStringLiteral("missing-extension")),
              directory.filePath(QStringLiteral("bad") + QChar::Null + QStringLiteral(".png"))}) {
            QVERIFY(!player.takeScreenshot(path));
            QVERIFY(!player.screenshotPending());
        }
        QVERIFY(finished.isEmpty());
        QVERIFY(pending.isEmpty());
        QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
    }

    void nativeWriteFailureReportsFailureAndAllowsRetry() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MpvController player(true, true);
        loadPausedVideo(player);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        QSignalSpy mediaFailures(&player, &MpvController::mediaLoadFailed);
        const auto badPath = directory.filePath(QStringLiteral("missing/frame.png"));
        QVERIFY(player.takeScreenshot(badPath));
        QVERIFY(player.screenshotPending());
        QVERIFY(finished.isEmpty());
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toString(), badPath);
        QVERIFY(!finished.at(0).at(1).toBool());
        QVERIFY(!player.screenshotPending());
        QVERIFY(!QFileInfo::exists(badPath));
        QCOMPARE(errors.count(), 1);
        QVERIFY(mediaFailures.isEmpty());
        QVERIFY(player.state().active);
        const auto retryPath = directory.filePath(QStringLiteral("retry.png"));
        QVERIFY(player.takeScreenshot(retryPath));
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(finished.at(1).at(1).toBool());
        QCOMPARE(QImage(retryPath).size(), QSize(320, 180));
        QCOMPARE(errors.count(), 1);
        QVERIFY(mediaFailures.isEmpty());
    }

    void subtitleFlagChangesPixelsAtTheSamePausedFrame() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MpvController player(true, true);
        loadPausedVideo(player, QString::fromUtf8(TEST_TRACK_MEDIA_PATH));
        QTRY_VERIFY(std::any_of(
            player.tracks().cbegin(), player.tracks().cend(),
            [](const MediaTrack &track) { return track.type == MediaTrack::Type::Subtitle; }));
        const auto subtitle = std::find_if(
            player.tracks().cbegin(), player.tracks().cend(),
            [](const MediaTrack &track) { return track.type == MediaTrack::Type::Subtitle; });
        player.selectSubtitleTrack(subtitle->id);
        QTRY_VERIFY(std::any_of(
            player.tracks().cbegin(), player.tracks().cend(), [](const MediaTrack &track) {
                return track.type == MediaTrack::Type::Subtitle && track.selected;
            }));
        player.setSubtitleVisible(true);
        player.seekAbsolute(1.0);
        QTRY_VERIFY(player.state().position >= 0.99 && player.state().position < 1.01);
        int seeking = 1;
        QTRY_VERIFY(mpv_get_property(player.handle(), "seeking", MPV_FORMAT_FLAG, &seeking) >= 0 &&
                    !seeking);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        const auto plainPath = directory.filePath(QStringLiteral("video.png"));
        const auto subtitlePath = directory.filePath(QStringLiteral("subtitles.png"));
        QVERIFY(player.takeScreenshot(plainPath, false));
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(1).toBool());
        QVERIFY(player.takeScreenshot(subtitlePath));
        QTRY_COMPARE(finished.count(), 2);
        QVERIFY(finished.at(1).at(1).toBool());
        const QImage plain(plainPath);
        const QImage subtitled(subtitlePath);
        QCOMPARE(plain.size(), QSize(320, 180));
        QCOMPARE(subtitled.size(), plain.size());
        QVERIFY(plain != subtitled);
        player.selectSubtitleTrack(0);
        QTRY_VERIFY(std::none_of(
            player.tracks().cbegin(), player.tracks().cend(), [](const MediaTrack &track) {
                return track.type == MediaTrack::Type::Subtitle && track.selected;
            }));
        const auto disabledPath = directory.filePath(QStringLiteral("disabled-subtitles.png"));
        QVERIFY(player.takeScreenshot(disabledPath));
        QTRY_COMPARE(finished.count(), 3);
        QVERIFY(finished.at(2).at(1).toBool());
        QCOMPARE(QImage(disabledPath), plain);
        QVERIFY(player.state().paused);
        QVERIFY(player.state().position >= 0.99 && player.state().position < 1.01);
    }

    void rejectsIdleAudioOnlyAndPendingMediaChanges() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("rejected.png"));
        MpvController player(true, true);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        QVERIFY(!player.canTakeScreenshot());
        QVERIFY(!player.takeScreenshot(path));
        loadPausedVideo(player);
        player.openFile(QString::fromUtf8(TEST_AUDIO_ONLY_PATH));
        QVERIFY(!player.canTakeScreenshot());
        QVERIFY(!player.takeScreenshot(path));
        QTRY_VERIFY(player.state().active);
        QTRY_VERIFY(!player.mediaInformation().audioCodec.isEmpty());
        QVERIFY(!player.canTakeScreenshot());
        QVERIFY(!player.takeScreenshot(path));
        player.openFile(QString::fromUtf8(TEST_MEDIA_PATH));
        QVERIFY(!player.canTakeScreenshot());
        QTRY_VERIFY(player.canTakeScreenshot());
        player.stop();
        QVERIFY(!player.canTakeScreenshot());
        QVERIFY(!player.takeScreenshot(path));
        QVERIFY(finished.isEmpty());
        QVERIFY(!QFileInfo::exists(path));
    }

    void pendingOperationSurvivesStopUntilItsOwnReply() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        MpvController player(true, true);
        loadPausedVideo(player);
        QSignalSpy finished(&player, &MpvController::screenshotFinished);
        QSignalSpy errors(&player, &MpvController::errorOccurred);
        const auto path = directory.filePath(QStringLiteral("before-stop.png"));
        QVERIFY(player.takeScreenshot(path));
        player.stop();
        QVERIFY(player.screenshotPending());
        QVERIFY(!player.takeScreenshot(directory.filePath(QStringLiteral("second.png"))));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(0).toString(), path);
        QVERIFY(!player.screenshotPending());
        if (finished.at(0).at(1).toBool()) {
            QCOMPARE(QImage(path).size(), QSize(320, 180));
            QVERIFY(errors.isEmpty());
        } else {
            // stop() suppresses the write error from the user-facing stream; the
            // operation still finishes and clears its pending state.
            QVERIFY(errors.isEmpty());
        }
    }
};

QTEST_GUILESS_MAIN(ScreenshotTest)
#include "screenshot_test.moc"
