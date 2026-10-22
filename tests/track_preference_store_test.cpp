#include "session/trackpreferencestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>

class TrackPreferenceStoreTest final : public QObject {
    Q_OBJECT
  private:
    static TrackPreferenceEntry entry(const QString &path, qint64 timestamp) {
        TrackPreferenceEntry result;
        result.mediaPath = path;
        const QFileInfo info(path);
        result.fileSize = info.size();
        result.modifiedUtcMs = info.lastModified().toUTC().toMSecsSinceEpoch();
        result.lastUsedUtcMs = timestamp;
        result.selection.audioTrackId = 2;
        result.selection.subtitleTrackId = 4;
        result.selection.externalSubtitlePaths = {path + QStringLiteral(".srt")};
        return result;
    }

  private slots:
    void roundTripsSelectionsAndFingerprints() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile mediaFile(media);
        QVERIFY(mediaFile.open(QIODevice::WriteOnly));
        QCOMPARE(mediaFile.write("video"), qint64(5));
        mediaFile.close();

        TrackPreferenceStore store(temporary.filePath(QStringLiteral("session.ini.tracks.ini")));
        auto saved = entry(media, 1700000000001LL);
        const auto result = store.save({saved});
        QVERIFY2(result.isEmpty(), qPrintable(result));
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().mediaPath, QFileInfo(media).canonicalFilePath());
        QCOMPARE(loaded.entries.front().fileSize, qint64(5));
        QCOMPARE(loaded.entries.front().selection.audioTrackId, std::optional<qint64>(2));
        QCOMPARE(loaded.entries.front().selection.subtitleTrackId, std::optional<qint64>(4));
        QCOMPARE(loaded.entries.front().selection.externalSubtitlePaths,
                 QStringList{media + QStringLiteral(".srt")});
    }

    void duplicateCanonicalMediaPathsKeepNewestSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile file(media);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("v"), qint64(1));
        file.close();
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("folder")));

        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        auto older = entry(media, 100);
        auto newer = entry(temporary.path() + QStringLiteral("/folder/../movie.mkv"), 200);
        newer.selection.audioTrackId = 9;
        QVERIFY(store.save({older, newer}).isEmpty());
        const auto loaded = store.load();
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().selection.audioTrackId, std::optional<qint64>(9));
        QCOMPARE(loaded.entries.front().lastUsedUtcMs, qint64(200));
    }

    void newestHundredMediaEntriesAreRetained() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QList<TrackPreferenceEntry> entries;
        for (int index = 0; index < 105; ++index) {
            const auto path = temporary.filePath(QStringLiteral("%1.mkv").arg(index));
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("v"), qint64(1));
            file.close();
            entries.append(entry(path, index + 1));
        }
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(store.save(entries).isEmpty());
        const auto loaded = store.load();
        QCOMPARE(loaded.entries.size(), 100);
        QCOMPARE(loaded.entries.front().lastUsedUtcMs, qint64(105));
        QVERIFY(std::none_of(loaded.entries.cbegin(), loaded.entries.cend(),
                             [](const auto &item) { return item.lastUsedUtcMs == 1; }));
    }

    void invalidMediaAndSelectionValuesAreRejected() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(
            !store.save({entry(temporary.path() + QStringLiteral("/missing.mkv"), 1)}).isEmpty());
        auto invalid = entry(temporary.path() + QStringLiteral("/relative.mkv"), 1);
        invalid.mediaPath = QStringLiteral("relative.mkv");
        QVERIFY(!store.save({invalid}).isEmpty());
    }

    void unknownSchemaIsPreservedAndCannotBeOverwritten() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 77);
            settings.setValue(QStringLiteral("future/value"), QStringLiteral("keep"));
            settings.sync();
        }
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll();
        original.close();
        TrackPreferenceStore store(path);
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), bytes);
    }

    void malformedSelectionFieldsArePreserved_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QVariant>("value");
        QTest::newRow("stream-text") << QStringLiteral("selectedExternalSubtitleStream")
                                     << QVariant(QStringLiteral("not-a-number"));
        QTest::newRow("stream-overflow")
            << QStringLiteral("selectedExternalSubtitleStream") << QVariant(qint64(2147483648LL));
        QTest::newRow("audio-presence")
            << QStringLiteral("audioTrack/present") << QVariant(QStringLiteral("garbage"));
        QTest::newRow("subtitle-presence")
            << QStringLiteral("subtitleTrack/present") << QVariant(QStringLiteral("garbage"));
    }

    void malformedSelectionFieldsArePreserved() {
        QFETCH(QString, key);
        QFETCH(QVariant, value);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile file(media);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("v"), qint64(1));
        file.close();
        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        const TrackPreferenceStore store(path);
        QVERIFY(store.save({entry(media, 1)}).isEmpty());
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("entries/0/") + key, value);
            settings.sync();
        }
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll();
        original.close();
        QVERIFY(!store.load().error.isEmpty());
        QVERIFY(!store.save({}).isEmpty());
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), bytes);
    }

    void missingAndChangedMediaDoNotInvalidateOtherEntries() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto missing = temporary.filePath(QStringLiteral("missing.mkv"));
        const auto changed = temporary.filePath(QStringLiteral("changed.mkv"));
        const auto retained = temporary.filePath(QStringLiteral("retained.mkv"));
        for (const auto &path : {missing, changed, retained}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("video"), qint64(5));
        }
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(store.save({entry(missing, 1), entry(changed, 2), entry(retained, 3)}).isEmpty());
        QVERIFY(QFile::remove(missing));
        QFile file(changed);
        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write("different"), qint64(9));
        file.close();
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().mediaPath, retained);
        QVERIFY(store.save(loaded.entries).isEmpty());
    }

    void savingPrunesStaleEntries() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto stale = temporary.filePath(QStringLiteral("stale.mkv"));
        const auto current = temporary.filePath(QStringLiteral("current.mkv"));
        for (const auto &path : {stale, current}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("v"), qint64(1));
        }
        const auto staleEntry = entry(stale, 1);
        QVERIFY(QFile::remove(stale));
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(store.save({staleEntry, entry(current, 2)}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().mediaPath, current);
    }
};

QTEST_GUILESS_MAIN(TrackPreferenceStoreTest)
#include "track_preference_store_test.moc"
