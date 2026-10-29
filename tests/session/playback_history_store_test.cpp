#include "session/playbackhistorystore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <limits>

class PlaybackHistoryStoreTest final : public QObject {
    Q_OBJECT
  private:
    static PlaybackHistoryEntry entry(const QString &path, double position = 12.5,
                                      double duration = 80.0, qint64 timestamp = 1000) {
        return {path, position, duration, timestamp};
    }

  private slots:
    void pathIsCleanedWithoutRequiringFileToExist() {
        const auto path = QStringLiteral("/tmp/missing/../media.mkv");
        QCOMPARE(PlaybackHistoryStore::normalizedPath(path), QStringLiteral("/tmp/media.mkv"));
        QVERIFY(PlaybackHistoryStore::normalizedPath(QStringLiteral("relative.mkv")).isEmpty());
        QVERIFY(PlaybackHistoryStore::normalizedPath(QString{}).isEmpty());
        QVERIFY(PlaybackHistoryStore::normalizedPath(QStringLiteral("/tmp/a\0b.mkv")).isEmpty());
    }

    void roundTripsEntriesAndUsesCleanAbsolutePathAsKey() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        PlaybackHistoryStore store(temporary.filePath(QStringLiteral("history.ini")));
        const auto missingPath = temporary.path() + QStringLiteral("/folder/../missing.mkv");
        const auto saved = entry(missingPath, 22.25, 125.0, 1700000000000LL);
        QVERIFY(store.save({saved}).isEmpty());
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().path,
                 QDir::cleanPath(temporary.path() + QStringLiteral("/missing.mkv")));
        QCOMPARE(loaded.entries.front().position, saved.position);
        QCOMPARE(loaded.entries.front().duration, saved.duration);
        QCOMPARE(loaded.entries.front().lastPlayedUtcMs, saved.lastPlayedUtcMs);
    }

    void repeatedIdenticalSavesLeaveFileUntouched() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("history.ini"));
        PlaybackHistoryStore store(path);
        const auto saved =
            entry(temporary.path() + QStringLiteral("/movie.mkv"), 22.25, 125.0, 1700000000000LL);
        QVERIFY(store.save({saved}).isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        file.close();

        const auto frozen = QDateTime::currentDateTimeUtc().addSecs(-3600);
        QFile timestampFile(path);
        QVERIFY(timestampFile.open(QIODevice::ReadWrite));
        QVERIFY(timestampFile.setFileTime(frozen, QFileDevice::FileModificationTime));
        timestampFile.close();
        QTest::qWait(10);
        QVERIFY(store.save({saved}).isEmpty());
        QCOMPARE(QFileInfo(path).lastModified().toUTC().toMSecsSinceEpoch(),
                 frozen.toMSecsSinceEpoch());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
        file.close();
    }

    void duplicatePathsAreReplacedByNewestEntry() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        PlaybackHistoryStore store(temporary.filePath(QStringLiteral("history.ini")));
        const auto path = temporary.path() + QStringLiteral("/movie.mkv");
        QVERIFY(store.save({entry(path, 10, 100, 100), entry(path, 40, 100, 200)}).isEmpty());
        const auto loaded = store.load();
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().position, 40.0);
        QCOMPARE(loaded.entries.front().lastPlayedUtcMs, qint64(200));
    }

    void newestHundredEntriesAreRetained() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        PlaybackHistoryStore store(temporary.filePath(QStringLiteral("history.ini")));
        QList<PlaybackHistoryEntry> entries;
        for (int i = 0; i < 105; ++i)
            entries.append(
                entry(temporary.path() + QStringLiteral("/%1.mkv").arg(i), i, 100, i + 1));
        QVERIFY(store.save(entries).isEmpty());
        const auto loaded = store.load();
        QCOMPARE(loaded.entries.size(), 100);
        QVERIFY(std::none_of(loaded.entries.cbegin(), loaded.entries.cend(),
                             [](const auto &item) { return item.lastPlayedUtcMs == 1; }));
        QCOMPARE(loaded.entries.front().lastPlayedUtcMs, qint64(105));
    }

    void saveRejectsInvalidEntryValues() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        PlaybackHistoryStore store(temporary.filePath(QStringLiteral("history.ini")));
        const auto path = temporary.path() + QStringLiteral("/movie.mkv");
        QVERIFY(!store.save({entry(QStringLiteral("relative.mkv"))}).isEmpty());
        QVERIFY(!store.save({entry(path, -1)}).isEmpty());
        QVERIFY(!store.save({entry(path, std::numeric_limits<double>::infinity())}).isEmpty());
        QVERIFY(!store.save({entry(path, 1, 0)}).isEmpty());
        QVERIFY(!store.save({entry(path, 1, std::numeric_limits<double>::quiet_NaN())}).isEmpty());
        QVERIFY(!store.save({entry(path, 1, 10, -1)}).isEmpty());
    }

    void loadQuarantinesInvalidEntriesAndAllowsRewrite() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto file = temporary.filePath(QStringLiteral("history.ini"));
        {
            QSettings settings(file, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("entries/size"), 4);
            settings.setValue(QStringLiteral("entries/0/path"), temporary.path() + "/valid.mkv");
            settings.setValue(QStringLiteral("entries/0/position"), 12.0);
            settings.setValue(QStringLiteral("entries/0/duration"), 80.0);
            settings.setValue(QStringLiteral("entries/0/lastPlayedUtcMs"), 1000);
            settings.setValue(QStringLiteral("entries/1/path"), QStringLiteral("relative.mkv"));
            settings.setValue(QStringLiteral("entries/1/position"), 12.0);
            settings.setValue(QStringLiteral("entries/1/duration"), 80.0);
            settings.setValue(QStringLiteral("entries/1/lastPlayedUtcMs"), 1000);
            settings.setValue(QStringLiteral("entries/2/path"), temporary.path() + "/negative.mkv");
            settings.setValue(QStringLiteral("entries/2/position"), -1.0);
            settings.setValue(QStringLiteral("entries/2/duration"), 80.0);
            settings.setValue(QStringLiteral("entries/2/lastPlayedUtcMs"), 1000);
            settings.setValue(QStringLiteral("entries/3/path"), temporary.path() + "/bad-time.mkv");
            settings.setValue(QStringLiteral("entries/3/position"), 12.0);
            settings.setValue(QStringLiteral("entries/3/duration"), 80.0);
            settings.setValue(QStringLiteral("entries/3/lastPlayedUtcMs"), -1);
            settings.sync();
        }
        PlaybackHistoryStore store(file);
        const auto loaded = store.load();
        // Per-entry corruption is quarantined instead of latching the store.
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().path, temporary.path() + QStringLiteral("/valid.mkv"));
        QVERIFY2(store.save({entry(temporary.path() + QStringLiteral("/other.mkv"))}).isEmpty(),
                 "a quarantined entry must not block the next save");
        const auto rewritten = store.load();
        QVERIFY2(rewritten.error.isEmpty(), qPrintable(rewritten.error));
        QCOMPARE(rewritten.entries.size(), 1);
        QCOMPARE(rewritten.entries.front().path, temporary.path() + QStringLiteral("/other.mkv"));
    }

    void corruptAndUnknownSchemaFilesArePreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QByteArray corrupt("[General]\nschemaVersion=1\n[broken group\nvalue=keep\n");
        const auto malformedPath = temporary.filePath(QStringLiteral("broken.ini"));
        QFile malformed(malformedPath);
        QVERIFY(malformed.open(QIODevice::WriteOnly));
        QCOMPARE(malformed.write(corrupt), corrupt.size());
        malformed.close();
        QVERIFY(!PlaybackHistoryStore(malformedPath).load().error.isEmpty());
        QVERIFY(!PlaybackHistoryStore(malformedPath).save({}).isEmpty());
        QVERIFY(malformed.open(QIODevice::ReadOnly));
        QCOMPARE(malformed.readAll(), corrupt);

        const auto unknownPath = temporary.filePath(QStringLiteral("future.ini"));
        {
            QSettings settings(unknownPath, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 42);
            settings.setValue(QStringLiteral("future/value"), QStringLiteral("keep"));
            settings.sync();
        }
        QFile unknown(unknownPath);
        QVERIFY(unknown.open(QIODevice::ReadOnly));
        const auto original = unknown.readAll();
        unknown.close();
        QVERIFY(!PlaybackHistoryStore(unknownPath).load().error.isEmpty());
        QVERIFY(!PlaybackHistoryStore(unknownPath).save({}).isEmpty());
        QVERIFY(unknown.open(QIODevice::ReadOnly));
        QCOMPARE(unknown.readAll(), original);
    }
};

QTEST_GUILESS_MAIN(PlaybackHistoryStoreTest)
#include "playback_history_store_test.moc"
