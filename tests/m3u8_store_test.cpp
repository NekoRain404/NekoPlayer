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
        QCOMPARE(loaded.paths.at(0), QDir(temporary.path()).absoluteFilePath(QStringLiteral("猫.mkv")));
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

    void rejectsInvalidOrOutOfRangeCurrentMarker() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("invalid.m3u8"));
        for (const auto &marker : {QByteArray("#NEKOPLAYER-CURRENT:x\n"),
                                   QByteArray("#NEKOPLAYER-CURRENT:2\na.mkv\n"),
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
        QVERIFY(!M3u8Store::load(temporary.filePath(QStringLiteral("missing.m3u8"))).error.isEmpty());
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
};

QTEST_GUILESS_MAIN(M3u8StoreTest)
#include "m3u8_store_test.moc"
