#include "preview/thumbnailservice.h"

#include <QSignalSpy>
#include <QtTest>
#include <type_traits>
#include <utility>

static_assert(
    std::is_same_v<decltype(std::declval<const ThumbnailService &>().cacheSize()), qsizetype>);

class ThumbnailServiceTest final : public QObject {
    Q_OBJECT

  private slots:
    void rejectsNetworkSourcesWithoutStartingADecode() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto requestId =
            service.request(QStringLiteral("https://example.com/video.mp4"), 1.0);

        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 2000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), requestId);
        const auto result = qvariant_cast<ThumbnailResult>(ready.at(0).at(3));
        QVERIFY(!result.succeeded());
        QVERIFY(result.error.contains(QStringLiteral("local"), Qt::CaseInsensitive));
    }

    void latestRequestSuppressesOlderResult() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto oldId = service.request(QString::fromUtf8(TEST_MEDIA_PATH), 0.0, QSize(160, 90));
        const auto latestId =
            service.request(QString::fromUtf8(TEST_MEDIA_PATH), 1.0, QSize(160, 90));

        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), latestId);
        QVERIFY(oldId < latestId);
        const auto result = qvariant_cast<ThumbnailResult>(ready.at(0).at(3));
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(result.image.width() <= 160);
        QVERIFY(result.image.height() <= 90);
    }

    void cancellationSuppressesPendingResult() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto requestId = service.request(QString::fromUtf8(TEST_MEDIA_PATH), 2.0);
        service.cancel();

        QTest::qWait(300);
        QVERIFY(requestId > 0);
        QCOMPARE(ready.count(), 0);
    }

    void repeatedRequestsForTheSameSecondReuseTheInFlightDecode() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto firstId = service.request(media, 1.2, QSize(160, 90));
        // The UI is second-granular: both requests target second 1.
        const auto repeatedId = service.request(media, 1.8, QSize(160, 90));
        QCOMPARE(repeatedId, firstId);

        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), firstId);
        QCOMPARE(ready.at(0).at(2).toDouble(), 1.0);
        const auto result = qvariant_cast<ThumbnailResult>(ready.at(0).at(3));
        QVERIFY2(result.succeeded(), qPrintable(result.error));
        QVERIFY(result.timestamp >= 1.0);
        QVERIFY(result.timestamp <= 1.2);
    }

    void changingTheQuantizedSecondRestartsTheDecode() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto firstId = service.request(media, 1.2, QSize(160, 90));
        const auto secondId = service.request(media, 2.2, QSize(160, 90));
        QVERIFY(secondId > firstId);

        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), secondId);
        QCOMPARE(ready.at(0).at(2).toDouble(), 2.0);
        const auto result = qvariant_cast<ThumbnailResult>(ready.at(0).at(3));
        QVERIFY2(result.succeeded(), qPrintable(result.error));
    }

    void cacheKeyUsesWholeSeconds() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto media = QString::fromUtf8(TEST_MEDIA_PATH);
        const auto firstId = service.request(media, 1.2);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), firstId);
        QCOMPARE(service.cacheSize(), 1);

        const auto cachedId = service.request(media, 1.9);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 2, 2000);
        QCOMPARE(ready.at(1).at(0).toULongLong(), cachedId);
        QCOMPARE(ready.at(1).at(2).toDouble(), 1.0);
        QCOMPARE(service.cacheSize(), 1);
    }

    void successfulResultsAreCachedAndBounded() {
        ThumbnailService service;
        QSignalSpy ready(&service, &ThumbnailService::thumbnailReady);
        const auto firstId = service.request(QString::fromUtf8(TEST_MEDIA_PATH), 0.0);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), firstId);
        QCOMPARE(service.cacheSize(), 1);

        const auto cachedId = service.request(QString::fromUtf8(TEST_MEDIA_PATH), 0.0);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 2, 2000);
        QCOMPARE(ready.at(1).at(0).toULongLong(), cachedId);
        QCOMPARE(service.cacheSize(), 1);
    }
};

QTEST_GUILESS_MAIN(ThumbnailServiceTest)
#include "thumbnail_service_test.moc"
