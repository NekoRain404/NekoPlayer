#include "session/trackpreferencestore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <algorithm>
#include <limits>

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
        result.hasTuning = true;
        result.playbackSpeed = 1.25;
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
        saved.selection.audioTrackId.reset();
        saved.selection.externalAudioPaths = {media + QStringLiteral(".mka")};
        saved.selection.selectedExternalAudioPath = saved.selection.externalAudioPaths.front();
        saved.selection.selectedExternalAudioStream = 1;
        const auto result = store.save({saved});
        QVERIFY2(result.isEmpty(), qPrintable(result));
        // Signed URLs may be stored in the selection; keep the file private.
        const auto permissions =
            QFileInfo(temporary.filePath(QStringLiteral("session.ini.tracks.ini"))).permissions();
        QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
        QVERIFY(!permissions.testFlag(QFileDevice::WriteOther));
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().mediaPath, QFileInfo(media).canonicalFilePath());
        QCOMPARE(loaded.entries.front().fileSize, qint64(5));
        QVERIFY(!loaded.entries.front().selection.audioTrackId.has_value());
        QCOMPARE(loaded.entries.front().selection.externalAudioPaths,
                 saved.selection.externalAudioPaths);
        QCOMPARE(loaded.entries.front().selection.selectedExternalAudioPath,
                 saved.selection.selectedExternalAudioPath);
        QCOMPARE(loaded.entries.front().selection.selectedExternalAudioStream, 1);
        QCOMPARE(loaded.entries.front().selection.subtitleTrackId, std::optional<qint64>(4));
        QCOMPARE(loaded.entries.front().selection.externalSubtitlePaths,
                 QStringList{media + QStringLiteral(".srt")});
    }

    void tuningValuesRoundTrip() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile mediaFile(media);
        QVERIFY(mediaFile.open(QIODevice::WriteOnly));
        QCOMPARE(mediaFile.write("video"), qint64(5));
        mediaFile.close();

        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        auto saved = entry(media, 1700000000001LL);
        saved.playbackSpeed = 1.5;
        saved.audioDelay = -0.25;
        saved.subtitleDelay = 2.75;
        QVERIFY(store.save({saved}).isEmpty());
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QVERIFY(loaded.entries.front().hasTuning);
        QCOMPARE(loaded.entries.front().playbackSpeed, 1.5);
        QCOMPARE(loaded.entries.front().audioDelay, -0.25);
        QCOMPARE(loaded.entries.front().subtitleDelay, 2.75);
        QCOMPARE(loaded.entries.front().selection.audioTrackId, std::optional<qint64>(2));
    }

    void legacyEntryWithoutTuningLoadsWithDefaults() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile mediaFile(media);
        QVERIFY(mediaFile.open(QIODevice::WriteOnly));
        QCOMPARE(mediaFile.write("video"), qint64(5));
        mediaFile.close();

        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        TrackPreferenceStore store(path);
        QVERIFY(store.save({entry(media, 1)}).isEmpty());
        {
            // Simulate a file written before tuning existed.
            QSettings settings(path, QSettings::IniFormat);
            settings.remove(QStringLiteral("entries/0/tuning"));
            settings.sync();
        }
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        QVERIFY(!loaded.entries.front().hasTuning);
        QCOMPARE(loaded.entries.front().playbackSpeed, 1.0);
        QCOMPARE(loaded.entries.front().audioDelay, 0.0);
        QCOMPARE(loaded.entries.front().subtitleDelay, 0.0);
        QCOMPARE(loaded.entries.front().selection.subtitleTrackId, std::optional<qint64>(4));
    }

    void invalidTuningValuesCannotBeSavedOrLoaded() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile mediaFile(media);
        QVERIFY(mediaFile.open(QIODevice::WriteOnly));
        QCOMPARE(mediaFile.write("video"), qint64(5));
        mediaFile.close();
        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        TrackPreferenceStore store(path);
        const auto saved = entry(media, 1);
        QVERIFY(store.save({saved}).isEmpty());
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto original = file.readAll();
        file.close();

        const auto rejected = [&](double speed, double audioDelay, double subtitleDelay) {
            auto candidate = saved;
            candidate.playbackSpeed = speed;
            candidate.audioDelay = audioDelay;
            candidate.subtitleDelay = subtitleDelay;
            QVERIFY(!store.save({candidate}).isEmpty());
        };
        rejected(5.0, 0, 0);
        rejected(0.1, 0, 0);
        rejected(1.0, 11.0, 0);
        rejected(1.0, 0, -601.0);
        rejected(std::numeric_limits<double>::quiet_NaN(), 0, 0);
        rejected(1.0, std::numeric_limits<double>::infinity(), 0);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), original);
        file.close();
    }

    void repeatedIdenticalSavesLeaveFileUntouched() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile mediaFile(media);
        QVERIFY(mediaFile.open(QIODevice::WriteOnly));
        QCOMPARE(mediaFile.write("video"), qint64(5));
        mediaFile.close();

        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        TrackPreferenceStore store(path);
        const auto saved = entry(media, 1700000000001LL);
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
        QTest::newRow("audio-stream-text")
            << QStringLiteral("selectedExternalAudioStream") << QVariant(QStringLiteral("x"));
        QTest::newRow("audio-stream-negative")
            << QStringLiteral("selectedExternalAudioStream") << QVariant(-2);
        QTest::newRow("audio-stream-large")
            << QStringLiteral("selectedExternalAudioStream") << QVariant(65536);
        QTest::newRow("audio-path-relative")
            << QStringLiteral("selectedExternalAudioPath") << QVariant(QStringLiteral("song.flac"));
        QTest::newRow("audio-path-long")
            << QStringLiteral("selectedExternalAudioPath")
            << QVariant(QStringLiteral("/") + QString(8192, QLatin1Char('a')));
        QTest::newRow("audio-path-null")
            << QStringLiteral("selectedExternalAudioPath")
            << QVariant(QStringLiteral("/audio") + QChar::Null + QStringLiteral(".flac"));
        QTest::newRow("audio-count-text")
            << QStringLiteral("externalAudioCount") << QVariant(QStringLiteral("x"));
        QTest::newRow("audio-count-negative")
            << QStringLiteral("externalAudioCount") << QVariant(-1);
        QTest::newRow("audio-count-large") << QStringLiteral("externalAudioCount") << QVariant(65);
        QTest::newRow("audio-path-missing") << QStringLiteral("externalAudioCount") << QVariant(1);
        QTest::newRow("tuning-presence")
            << QStringLiteral("tuning/present") << QVariant(QStringLiteral("garbage"));
        QTest::newRow("tuning-speed-large") << QStringLiteral("tuning/speed") << QVariant(5.0);
        QTest::newRow("tuning-speed-small") << QStringLiteral("tuning/speed") << QVariant(0.1);
        QTest::newRow("tuning-speed-nan")
            << QStringLiteral("tuning/speed") << QVariant(QStringLiteral("nan"));
        QTest::newRow("tuning-speed-missing") << QStringLiteral("tuning/speed") << QVariant();
        QTest::newRow("tuning-audio-large")
            << QStringLiteral("tuning/audioDelay") << QVariant(11.0);
        QTest::newRow("tuning-audio-inf")
            << QStringLiteral("tuning/audioDelay") << QVariant(QStringLiteral("inf"));
        QTest::newRow("tuning-subtitle-small")
            << QStringLiteral("tuning/subtitleDelay") << QVariant(-601.0);
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
        // Missing media keeps its entry (the drive may come back); replaced
        // media is dropped; unrelated entries survive either way.
        QCOMPARE(loaded.entries.size(), 2);
        const auto paths = [&loaded] {
            QStringList result;
            for (const auto &item : loaded.entries)
                result.append(item.mediaPath);
            result.sort();
            return result;
        }();
        QCOMPARE(paths, (QStringList{QDir::cleanPath(missing), retained}));
        QVERIFY(store.save(loaded.entries).isEmpty());
    }

    void legacySelectionWithoutAudioFieldsLoadsAndRetainsInternalChoice() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile file(media);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("v"), qint64(1));
        file.close();
        const auto path = temporary.filePath(QStringLiteral("tracks.ini"));
        TrackPreferenceStore store(path);
        QVERIFY(store.save({entry(media, 1)}).isEmpty());
        {
            QSettings settings(path, QSettings::IniFormat);
            for (const auto &key :
                 {QStringLiteral("selectedExternalAudioPath"),
                  QStringLiteral("selectedExternalAudioStream"),
                  QStringLiteral("externalAudioCount"), QStringLiteral("externalAudio")})
                settings.remove(QStringLiteral("entries/0/") + key);
            settings.sync();
        }
        const auto loaded = store.load();
        QVERIFY2(loaded.error.isEmpty(), qPrintable(loaded.error));
        QCOMPARE(loaded.entries.size(), 1);
        const auto &selection = loaded.entries.front().selection;
        QCOMPARE(selection.audioTrackId, std::optional<qint64>(2));
        QVERIFY(selection.selectedExternalAudioPath.isEmpty());
        QCOMPARE(selection.selectedExternalAudioStream, -1);
        QVERIFY(selection.externalAudioPaths.isEmpty());
    }

    void invalidExternalAudioSaveDoesNotOverwriteExistingSelection() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto media = temporary.filePath(QStringLiteral("movie.mkv"));
        QFile file(media);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("v"), qint64(1));
        file.close();
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        auto saved = entry(media, 1);
        saved.selection.audioTrackId = 0;
        QVERIFY(store.save({saved}).isEmpty());
        auto invalid = saved;
        invalid.selection.externalAudioPaths = {QStringLiteral("relative.flac")};
        QVERIFY(!store.save({invalid}).isEmpty());
        invalid = saved;
        invalid.selection.selectedExternalAudioStream = 65536;
        QVERIFY(!store.save({invalid}).isEmpty());
        invalid = saved;
        for (int index = 0; index < 65; ++index)
            invalid.selection.externalAudioPaths.append(media + QString::number(index));
        QVERIFY(!store.save({invalid}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().selection.audioTrackId, std::optional<qint64>(0));
    }

    void savingKeepsTemporarilyMissingEntries() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto offline = temporary.filePath(QStringLiteral("offline.mkv"));
        const auto current = temporary.filePath(QStringLiteral("current.mkv"));
        for (const auto &path : {offline, current}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("v"), qint64(1));
        }
        auto offlineEntry = entry(offline, 1);
        offlineEntry.playbackSpeed = 1.75;
        QVERIFY(QFile::remove(offline));
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(store.save({offlineEntry, entry(current, 2)}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.entries.size(), 2);
        const auto found = std::find_if(
            loaded.entries.cbegin(), loaded.entries.cend(),
            [&offline](const auto &item) { return item.mediaPath == QDir::cleanPath(offline); });
        QVERIFY(found != loaded.entries.cend());
        QCOMPARE(found->selection.subtitleTrackId, std::optional<qint64>(4));
        QVERIFY(found->hasTuning);
        QCOMPARE(found->playbackSpeed, 1.75);
    }

    void savingPrunesReplacedMedia() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto replaced = temporary.filePath(QStringLiteral("replaced.mkv"));
        const auto current = temporary.filePath(QStringLiteral("current.mkv"));
        for (const auto &path : {replaced, current}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("v"), qint64(1));
        }
        auto replacedEntry = entry(replaced, 1);
        replacedEntry.hasTuning = true;
        QFile file(replaced);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("different-content"), qint64(17));
        file.close();
        TrackPreferenceStore store(temporary.filePath(QStringLiteral("tracks.ini")));
        QVERIFY(store.save({replacedEntry, entry(current, 2)}).isEmpty());
        const auto loaded = store.load();
        QVERIFY(loaded.error.isEmpty());
        QCOMPARE(loaded.entries.size(), 1);
        QCOMPARE(loaded.entries.front().mediaPath, current);
    }
};

QTEST_GUILESS_MAIN(TrackPreferenceStoreTest)
#include "track_preference_store_test.moc"
