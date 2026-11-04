#include "playlist/playlistfile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class PlaylistFileTest final : public QObject {
    Q_OBJECT
  private slots:
    void loadsPlsWithMetadataAndRelativePaths() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("list.pls")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[playlist]\nNumberOfEntries=2\n"
                   "File1=episode 1.mkv\nTitle1=Pilot\nLength1=42\n"
                   "File2=https://example.test/episode2.mkv\nTitle2=Finale\nLength2=61\n");
        file.close();
        const auto result = PlaylistFile::load(file.fileName());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.paths.size(), 2);
        QCOMPARE(result.paths.at(0), QDir(directory.path()).absoluteFilePath("episode 1.mkv"));
        QCOMPARE(result.paths.at(1), QStringLiteral("https://example.test/episode2.mkv"));
        QCOMPARE(result.entries.at(0).title, QStringLiteral("Pilot"));
        QCOMPARE(result.entries.at(0).durationSeconds, 42.0);
        QCOMPARE(result.currentIndex, 0);
    }

    void plsRoundTripPreservesOrderAndMetadata() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QList<M3u8Entry> entries{
            {QDir(directory.path()).absoluteFilePath(QStringLiteral("a.mkv")), 12.5,
             QStringLiteral("A")},
            {QStringLiteral("https://example.test/b.mkv?token=1"), -1, {}}};
        const auto path = directory.filePath(QStringLiteral("out.pls"));
        QVERIFY2(PlaylistFile::save(path, entries, 1).isEmpty(), "save failed");
        const auto loaded = PlaylistFile::load(path);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 2);
        QCOMPARE(loaded.entries.at(0).title, QStringLiteral("A"));
        QCOMPARE(loaded.entries.at(0).durationSeconds, 12.0);
        QCOMPARE(loaded.entries.at(1).source, QStringLiteral("https://example.test/b.mkv?token=1"));
    }

    void xspfRoundTripPreservesOrderAndMetadata() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QList<M3u8Entry> entries{
            {QDir(directory.path()).absoluteFilePath(QStringLiteral("one.mkv")), 3.0,
             QStringLiteral("One")},
            {QStringLiteral("https://example.test/two.mkv"), 4.25, QStringLiteral("Two")}};
        const auto path = directory.filePath(QStringLiteral("out.xspf"));
        QVERIFY2(PlaylistFile::save(path, entries, 0).isEmpty(), "save failed");
        const auto loaded = PlaylistFile::load(path);
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 2);
        QCOMPARE(loaded.entries.at(0).title, QStringLiteral("One"));
        QCOMPARE(loaded.entries.at(1).source, QStringLiteral("https://example.test/two.mkv"));
        QVERIFY(std::abs(loaded.entries.at(1).durationSeconds - 4.25) < 0.001);
    }

    void malformedXspfReportsAnError() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile file(directory.filePath(QStringLiteral("broken.xspf")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<playlist><trackList><track>");
        file.close();
        const auto loaded = PlaylistFile::load(file.fileName());
        QVERIFY(!loaded.error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(PlaylistFileTest)
#include "playlist_file_test.moc"
