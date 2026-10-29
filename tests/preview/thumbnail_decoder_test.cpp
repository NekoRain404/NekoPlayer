#include "preview/ffmpegthumbnaildecoder.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <atomic>
#include <memory>

class ThumbnailDecoderTest final : public QObject {
    Q_OBJECT
  private slots:
    void decodesAFrameWithoutTouchingMpv() {
        ThumbnailRequest request;
        request.source = QString::fromUtf8(TEST_MEDIA_PATH);
        request.seconds = 1.0;
        request.maximumSize = QSize(160, 90);

        const auto result = decodeThumbnail(request);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(!result.image.isNull());
        QVERIFY(result.image.width() <= 160);
        QVERIFY(result.image.height() <= 90);
        QVERIFY(result.image.width() > 0);
        QVERIFY(result.image.height() > 0);
        QVERIFY(result.timestamp >= 0.0);
    }

    void seeksBeyondThePreviousKeyframe() {
        ThumbnailRequest request;
        request.source = QString::fromUtf8(TEST_MEDIA_PATH);
        request.seconds = 1.1;
        request.maximumSize = QSize(160, 90);

        const auto result = decodeThumbnail(request);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(result.timestamp >= 1.0);
        QVERIFY(result.timestamp <= 1.2);
    }

    void reportsMediaRelativeTimeForNonzeroStreamStart() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto source = directory.filePath(QStringLiteral("offset.mkv"));
        QProcess ffmpeg;
        ffmpeg.start(QString::fromUtf8(FFMPEG_EXECUTABLE),
                     {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
                      QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-f"),
                      QStringLiteral("lavfi"), QStringLiteral("-i"),
                      QStringLiteral("testsrc2=size=64x64:rate=5"), QStringLiteral("-t"),
                      QStringLiteral("2"), QStringLiteral("-c:v"), QStringLiteral("mpeg4"),
                      QStringLiteral("-output_ts_offset"), QStringLiteral("10"), source});
        QVERIFY(ffmpeg.waitForStarted());
        QVERIFY(ffmpeg.waitForFinished(10000));
        QCOMPARE(ffmpeg.exitStatus(), QProcess::NormalExit);
        QCOMPARE(ffmpeg.exitCode(), 0);

        ThumbnailRequest request;
        request.source = source;
        request.seconds = 0.8;
        request.maximumSize = QSize(64, 64);
        const auto result = decodeThumbnail(request);
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(result.timestamp >= 0.8);
        QVERIFY(result.timestamp <= 1.0);
    }

    void rejectsMalformedMediaWithoutLeakingTheFormatContext() {
        QTemporaryFile malformed(QDir::tempPath() + QStringLiteral("/neko-thumbnail-XXXXXX.mkv"));
        QVERIFY(malformed.open());
        QVERIFY(malformed.write("not a media file") > 0);
        malformed.flush();

        ThumbnailRequest request;
        request.source = malformed.fileName();
        const auto result = decodeThumbnail(request);
        QVERIFY(!result.succeeded());
        QVERIFY(!result.error.isEmpty());
    }

    void rejectsRemotePlaylistSourceWithoutNetworkAccess() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto source = directory.filePath(QStringLiteral("remote-playlist.m3u8"));
        QFile playlist(source);
        QVERIFY(playlist.open(QIODevice::WriteOnly | QIODevice::Truncate));
        // The HLS demuxer resolves segment URLs through the input's protocol
        // whitelist, so this local playlist referencing a remote segment must
        // be rejected before any connection is attempted.
        const QByteArray manifest("#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:10\n"
                                  "#EXTINF:10.0,\nhttps://example.com/seg0.ts\n#EXT-X-ENDLIST\n");
        QVERIFY(playlist.write(manifest) > 0);
        playlist.close();

        ThumbnailRequest request;
        request.source = source;
        QElapsedTimer elapsed;
        elapsed.start();
        const auto result = decodeThumbnail(request);
        QVERIFY(!result.succeeded());
        QVERIFY(!result.error.isEmpty());
        // The decoder's wall-clock deadline is five seconds; a blocked input
        // must be cut off well before the test timeout.
        QVERIFY(elapsed.elapsed() < 6000);
    }

    void rejectsInvalidAndCancelledRequests() {
        ThumbnailRequest missing;
        missing.source = QStringLiteral("/missing/thumbnail.mkv");
        QVERIFY(!decodeThumbnail(missing).succeeded());

        ThumbnailRequest cancelled;
        cancelled.source = QString::fromUtf8(TEST_MEDIA_PATH);
        cancelled.cancellation = std::make_shared<std::atomic_bool>(true);
        const auto result = decodeThumbnail(cancelled);
        QVERIFY(!result.succeeded());
        QVERIFY(result.error.contains(QStringLiteral("cancel"), Qt::CaseInsensitive));
    }
};

QTEST_GUILESS_MAIN(ThumbnailDecoderTest)
#include "thumbnail_decoder_test.moc"
