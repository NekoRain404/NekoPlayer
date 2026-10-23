#include "preview/ffmpegthumbnaildecoder.h"

#include <QDir>
#include <QFileInfo>
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
