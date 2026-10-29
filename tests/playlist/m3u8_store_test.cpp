#include "playlist/m3u8store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class M3u8StoreTest final : public QObject {
    Q_OBJECT
  private slots:
    void saveAndLoadPreserveOrderDuplicatesAndCurrent() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("列表.m3u8"));
        const QStringList paths{QStringLiteral("/media/猫.mkv"),
                                QStringLiteral("https://example.test/影片.mkv"),
                                QStringLiteral("/media/猫.mkv")};
        QVERIFY2(M3u8Store::save(file, paths, 1).isEmpty(), "save failed");

        const auto loaded = M3u8Store::load(file);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.paths, paths);
        QCOMPARE(loaded.currentIndex, 1);
    }

    void loadResolvesRelativeEntriesAndDefaultsToFirst() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("nested/list.m3u8"));
        QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QByteArray contents("#EXTM3U\n# ignored\n../猫.mkv\nhttps://example.test/a.mkv\n");
        QCOMPARE(output.write(contents), contents.size());
        output.close();

        const auto loaded = M3u8Store::load(file);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.paths.size(), 2);
        QCOMPARE(loaded.paths.at(0),
                 QDir(temporary.path()).absoluteFilePath(QStringLiteral("猫.mkv")));
        QCOMPARE(loaded.paths.at(1), QStringLiteral("https://example.test/a.mkv"));
        QCOMPARE(loaded.currentIndex, 0);
    }

    void emptyListHasNoCurrentItem() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("empty.m3u8"));
        QVERIFY(M3u8Store::save(file, {}, -1).isEmpty());
        const auto loaded = M3u8Store::load(file);
        QVERIFY(loaded.error.isEmpty());
        QVERIFY(loaded.paths.isEmpty());
        QCOMPARE(loaded.currentIndex, -1);
    }

    void rejectsHlsManifestInsteadOfFlatteningSegments() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("stream.m3u8"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QByteArray manifest("#EXTM3U\n#EXT-X-TARGETDURATION:4\n#EXTINF:4.0,\n"
                                  "segment0.ts\n#EXTINF:4.0,\nsegment1.ts\n");
        QCOMPARE(output.write(manifest), manifest.size());
        output.close();
        const auto loaded = M3u8Store::load(file);
        QVERIFY(!loaded.error.isEmpty());
        QVERIFY(loaded.paths.isEmpty());
        QVERIFY(loaded.error.contains(QStringLiteral("HLS")));
    }

    void extinfDurationsAndTitlesSurviveRoundTrip() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto input = temporary.filePath(QStringLiteral("input.m3u8"));
        {
            QFile file(input);
            QVERIFY(file.open(QIODevice::WriteOnly));
            const auto body = QStringLiteral("#EXTM3U\n#EXTINF:12.5,Episode Two\n%1\n"
                                             "#EXTINF:-1,Local Clip\n%2\n")
                                  .arg(temporary.filePath(QStringLiteral("b.mkv")),
                                       temporary.filePath(QStringLiteral("a.mkv")))
                                  .toUtf8();
            QCOMPARE(file.write(body), body.size());
        }
        const auto loaded = M3u8Store::load(input);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 2);
        QCOMPARE(loaded.entries.at(0).durationSeconds, 12.5);
        QCOMPARE(loaded.entries.at(0).title, QStringLiteral("Episode Two"));
        QCOMPARE(loaded.entries.at(1).durationSeconds, -1.0);
        QCOMPARE(loaded.entries.at(1).title, QStringLiteral("Local Clip"));

        const auto output = temporary.filePath(QStringLiteral("output.m3u8"));
        QVERIFY(M3u8Store::saveEntries(output, loaded.entries, 1).isEmpty());
        const auto again = M3u8Store::load(output);
        QVERIFY2(again.error.isEmpty(), qPrintable(again.error));
        QCOMPARE(again.entries, loaded.entries);
        QCOMPARE(again.currentIndex, 1);
        QCOMPARE(again.paths, loaded.paths);
    }

    void savesNonEmptyListWithoutCurrentRow() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("missing.mkv"));
        const auto file = temporary.filePath(QStringLiteral("queue.m3u8"));
        // A queue made only of missing files has rows but no current row; it
        // must still be saveable.
        QVERIFY2(M3u8Store::save(file, {media}, -1).isEmpty(),
                 "non-empty queue with no current row must be saveable");
        const auto loaded = M3u8Store::load(file);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.paths, QStringList{media});
        QCOMPARE(loaded.currentIndex, 0);
    }

    void rejectsInvalidOrOutOfRangeCurrentMarker() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("invalid.m3u8"));
        for (const auto &marker :
             {QByteArray("#NEKOPLAYER-CURRENT:x\n"), QByteArray("#NEKOPLAYER-CURRENT:2\na.mkv\n"),
              QByteArray("#NEKOPLAYER-CURRENT:0\n")}) {
            QFile output(file);
            QVERIFY(output.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(output.write(marker), marker.size());
            output.close();
            const auto loaded = M3u8Store::load(file);
            QVERIFY(!loaded.error.isEmpty());
            QVERIFY(loaded.paths.isEmpty());
        }
    }

    void rejectsMalformedCurrentMarkerPrefix() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("malformed-marker.m3u8"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QByteArray contents("#NEKOPLAYER-CURRENTx:0\na.mkv\n");
        QCOMPARE(output.write(contents), contents.size());
        output.close();

        const auto loaded = M3u8Store::load(file);
        QVERIFY(!loaded.error.isEmpty());
        QVERIFY(loaded.paths.isEmpty());
    }

    void readFailureAndInvalidUtf8ReturnErrors() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(
            !M3u8Store::load(temporary.filePath(QStringLiteral("missing.m3u8"))).error.isEmpty());
        const auto file = temporary.filePath(QStringLiteral("bad.m3u8"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        QCOMPARE(output.write(QByteArray("a.mkv\n\xff", 7)), qint64(7));
        output.close();
        QVERIFY(!M3u8Store::load(file).error.isEmpty());
    }

    void rejectsMalformedUrlEntries() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("bad-url.m3u8"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QByteArray contents("http://[\n");
        QCOMPARE(output.write(contents), contents.size());
        output.close();

        const auto loaded = M3u8Store::load(file);
        QVERIFY(!loaded.error.isEmpty());
        QVERIFY(loaded.paths.isEmpty());
    }

    void failedSaveDoesNotChangeExistingFile() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("existing.m3u8"));
        QFile output(file);
        QVERIFY(output.open(QIODevice::WriteOnly));
        const QByteArray original("preserve me\n");
        QCOMPARE(output.write(original), original.size());
        output.close();
        QVERIFY(!M3u8Store::save(file, {QStringLiteral("bad\nentry")}, 0).isEmpty());
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), original);
    }

    void rejectsUnplayableSourcesBeforePublishingAnyEntries_data() {
        QTest::addColumn<QString>("entry");
        QTest::newRow("hostless") << QStringLiteral("https:///movie.mkv");
        QTest::newRow("unsupported") << QStringLiteral("ftp://example.test/movie.mkv");
        QTest::newRow("bad-port") << QStringLiteral("http://example.test:99999/movie.mkv");
        QTest::newRow("bad-escape") << QStringLiteral("https://example.test/%zz");
        QTest::newRow("encoded-nul") << QStringLiteral("file:///tmp/%00movie.mkv");
        QTest::newRow("relative-file-url") << QStringLiteral("file:relative.mkv");
        QTest::newRow("file-query") << QStringLiteral("file:///tmp/movie.mkv?x=1");
    }

    void rejectsUnplayableSourcesBeforePublishingAnyEntries() {
        QFETCH(QString, entry);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("mixed.m3u8"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const auto contents = QByteArray("#EXTM3U\n#NEKOPLAYER-CURRENT:1\nvalid.mkv\n") +
                              entry.toUtf8() + "\nother.mkv\n";
        QCOMPARE(file.write(contents), contents.size());
        file.close();

        const auto loaded = M3u8Store::load(path);
        QVERIFY(!loaded.error.isEmpty());
        QVERIFY(loaded.paths.isEmpty());
        QCOMPARE(loaded.currentIndex, -1);
    }

    void invalidSourceSaveKeepsExistingFile_data() {
        rejectsUnplayableSourcesBeforePublishingAnyEntries_data();
    }

    void invalidSourceSaveKeepsExistingFile() {
        QFETCH(QString, entry);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("existing.m3u8"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray contents("#EXTM3U\noriginal.mkv\n");
        QCOMPARE(file.write(contents), contents.size());
        file.close();
        QVERIFY(!M3u8Store::save(path, {QStringLiteral("/tmp/valid.mkv"), entry}, 1).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), contents);
    }

    void fileUrlsDecodeBeforeEnteringTheQueueAndKeepDuplicateSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("local.m3u8"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray contents("#EXTM3U\n#NEKOPLAYER-CURRENT:2\n"
                                  "file:///tmp/%E7%8C%AB%20clip.mkv\n../other.mkv\n"
                                  "file:///tmp/%E7%8C%AB%20clip.mkv\n");
        QCOMPARE(file.write(contents), contents.size());
        file.close();

        const auto loaded = M3u8Store::load(path);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.paths,
                 QStringList({QStringLiteral("/tmp/猫 clip.mkv"),
                              QDir::cleanPath(temporary.filePath(QStringLiteral("../other.mkv"))),
                              QStringLiteral("/tmp/猫 clip.mkv")}));
        QCOMPARE(loaded.currentIndex, 2);
    }

    void signedNetworkEntriesSurviveRoundTripByteForByte() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("signed.m3u8"));
        const QStringList entries{
            QStringLiteral("https://example.test/%7e/%E7%8C%AB.mkv?sig=a%2fb%41&x=1&x=2"),
            QStringLiteral("rtsp://[::1]:8554/live?token=a%2fb"),
            QStringLiteral("rtmps://example.test/live/key")};
        QVERIFY(M3u8Store::save(path, entries, 1).isEmpty());
        const auto loaded = M3u8Store::load(path);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.paths, entries);
        QCOMPARE(loaded.currentIndex, 1);
    }
};

QTEST_GUILESS_MAIN(M3u8StoreTest)
#include "m3u8_store_test.moc"
