#include "session/recentmediahistory.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class RecentMediaHistoryTest final : public QObject {
    Q_OBJECT
  private:
    static bool write(const QString &path, const QByteArray &bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
    }

    static QByteArray read(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }

  private slots:
    void remembersNormalizedUniqueSourcesNewestFirst() {
        RecentMediaHistory history;
        QSignalSpy changed(&history, &RecentMediaHistory::changed);
        history.remember(QStringLiteral("/missing/folder/../first.mkv"));
        history.remember(QStringLiteral("/missing/second.mkv"));
        history.remember(QStringLiteral("file:///missing/first.mkv"));
        QCOMPARE(history.entries(), QStringList({QStringLiteral("/missing/first.mkv"),
                                                 QStringLiteral("/missing/second.mkv")}));
        QCOMPARE(changed.count(), 3);
        history.remember(QStringLiteral("/missing/first.mkv"));
        history.remember({});
        history.remember(QStringLiteral("ftp://example.invalid/movie.mkv"));
        history.remember(QStringLiteral("/missing/a\0b"));
        history.remember(QString(17000, QLatin1Char('x')));
        QCOMPARE(changed.count(), 3);
        QCOMPARE(history.entries().size(), 2);
    }

    void evictsOldestAfterTwentyDifferentSources() {
        RecentMediaHistory history;
        for (int index = 0; index < 25; ++index)
            history.remember(QStringLiteral("/missing/%1.mkv").arg(index));
        QCOMPARE(history.entries().size(), 20);
        QCOMPARE(history.entries().front(), QStringLiteral("/missing/24.mkv"));
        QCOMPARE(history.entries().back(), QStringLiteral("/missing/5.mkv"));
        history.remember(QStringLiteral("/missing/12.mkv"));
        QCOMPARE(history.entries().front(), QStringLiteral("/missing/12.mkv"));
        QCOMPARE(history.entries().size(), 20);
        QCOMPARE(history.entries().back(), QStringLiteral("/missing/5.mkv"));
    }

    void roundTripsSignedNetworkUrlsAndMissingFilesOnlyOnExplicitSave() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("nested/recent.json"));
        const auto signedUrl = QStringLiteral(
            "https://example.invalid/a%2fb/%7eclip.mkv?token=a%2fb&signature=%2F+%7e#part");
        RecentMediaHistory history;
        QVERIFY(history.load(path).isEmpty());
        history.remember(QStringLiteral("/missing/猫 film.mkv"));
        history.remember(signedUrl);
        QVERIFY(!QFile::exists(path));
        QVERIFY2(history.save().isEmpty(), qPrintable(history.save()));
        RecentMediaHistory restored;
        QSignalSpy changed(&restored, &RecentMediaHistory::changed);
        QVERIFY(restored.load(path).isEmpty());
        QCOMPARE(restored.entries(),
                 QStringList({signedUrl, QStringLiteral("/missing/猫 film.mkv")}));
        QCOMPARE(changed.count(), 1);
        QVERIFY(restored.load(path).isEmpty());
        QCOMPARE(changed.count(), 1);
        const auto object = QJsonDocument::fromJson(read(path)).object();
        QCOMPARE(object.value(QStringLiteral("schemaVersion")).toInt(), 1);
        QCOMPARE(object.value(QStringLiteral("entries")).toArray().size(), 2);
    }

    void clearPersistsAnEmptyListWithoutTouchingAnotherFile() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("recent.json"));
        const auto resume = temporary.filePath(QStringLiteral("resume.ini"));
        QVERIFY(write(resume, "resume-position=42\n"));
        RecentMediaHistory history;
        QVERIFY(history.load(path).isEmpty());
        history.remember(QStringLiteral("/missing/movie.mkv"));
        QVERIFY(history.save().isEmpty());
        const auto before = read(path);
        QSignalSpy changed(&history, &RecentMediaHistory::changed);
        history.clear();
        history.clear();
        QVERIFY(history.entries().isEmpty());
        QCOMPARE(changed.count(), 1);
        QCOMPARE(read(path), before);
        QCOMPARE(read(resume), QByteArray("resume-position=42\n"));
        QVERIFY(history.save().isEmpty());
        RecentMediaHistory restored;
        QVERIFY(restored.load(path).isEmpty());
        QVERIFY(restored.entries().isEmpty());
    }

    void rejectsInvalidDocumentsWithoutLosingMemoryOrOverwritingOriginal_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("malformed") << QByteArray("{broken json");
        QTest::newRow("empty") << QByteArray{};
        QTest::newRow("array-root") << QByteArray("[]");
        QTest::newRow("future") << QByteArray("{\"schemaVersion\":99,\"entries\":[]}");
        QTest::newRow("string-version") << QByteArray("{\"schemaVersion\":\"1\",\"entries\":[]}");
        QTest::newRow("fractional-version") << QByteArray("{\"schemaVersion\":1.2,\"entries\":[]}");
        QTest::newRow("missing-entries") << QByteArray("{\"schemaVersion\":1}");
        QTest::newRow("wrong-entries-type") << QByteArray("{\"schemaVersion\":1,\"entries\":{}}");
        QTest::newRow("wrong-item-type") << QByteArray("{\"schemaVersion\":1,\"entries\":[42]}");
        QTest::newRow("empty-item") << QByteArray("{\"schemaVersion\":1,\"entries\":[\"\"]}");
        QTest::newRow("unsupported-url")
            << QByteArray("{\"schemaVersion\":1,\"entries\":[\"ftp://example.invalid/a\"]}");
        QTest::newRow("unknown-field")
            << QByteArray("{\"schemaVersion\":1,\"entries\":[],\"future\":true}");
        QJsonArray tooMany;
        for (int index = 0; index < 21; ++index)
            tooMany.append(QStringLiteral("/missing/%1.mkv").arg(index));
        QTest::newRow("too-many-items")
            << QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1},
                                         {QStringLiteral("entries"), tooMany}})
                   .toJson();
        QTest::newRow("oversized-item")
            << QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 1},
                                         {QStringLiteral("entries"),
                                          QJsonArray{QStringLiteral("/") + QString(17000, 'x')}}})
                   .toJson();
        QTest::newRow("oversized-file") << QByteArray(4 * 1024 * 1024 + 1, ' ');
    }

    void rejectsInvalidDocumentsWithoutLosingMemoryOrOverwritingOriginal() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("recent.json"));
        QVERIFY(write(path, bytes));
        RecentMediaHistory history;
        history.remember(QStringLiteral("/missing/in-memory.mkv"));
        QSignalSpy changed(&history, &RecentMediaHistory::changed);
        QVERIFY(!history.load(path).isEmpty());
        QCOMPARE(history.entries(), QStringList{QStringLiteral("/missing/in-memory.mkv")});
        QCOMPARE(changed.count(), 0);
        history.clear();
        QVERIFY(!history.save().isEmpty());
        QCOMPARE(read(path), bytes);
    }

    void rejectsExternalCorruptionAtSaveTime_data() {
        QTest::addColumn<QByteArray>("replacement");
        QTest::newRow("malformed") << QByteArray("do not overwrite");
        QTest::newRow("future") << QByteArray("{\"schemaVersion\":2,\"entries\":[]}");
        QTest::newRow("wrong-item-type") << QByteArray("{\"schemaVersion\":1,\"entries\":[null]}");
    }

    void rejectsExternalCorruptionAtSaveTime() {
        QFETCH(QByteArray, replacement);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("recent.json"));
        RecentMediaHistory history;
        QVERIFY(history.load(path).isEmpty());
        history.remember(QStringLiteral("/missing/movie.mkv"));
        QVERIFY(history.save().isEmpty());
        QVERIFY(write(path, replacement));
        history.remember(QStringLiteral("/missing/other.mkv"));
        QVERIFY(!history.save().isEmpty());
        QCOMPARE(read(path), replacement);
    }

    void reportsUnreadableTargetAndFailedParentCreation() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        RecentMediaHistory directoryTarget;
        QVERIFY(!directoryTarget.load(temporary.path()).isEmpty());
        QVERIFY(!directoryTarget.save().isEmpty());
        const auto blocker = temporary.filePath(QStringLiteral("file"));
        QVERIFY(write(blocker, "keep"));
        RecentMediaHistory blockedParent;
        const auto path = blocker + QStringLiteral("/recent.json");
        QVERIFY(blockedParent.load(path).isEmpty());
        blockedParent.remember(QStringLiteral("/missing/movie.mkv"));
        QVERIFY(!blockedParent.save().isEmpty());
        QCOMPARE(read(blocker), QByteArray("keep"));
    }

    void emptyLoadPathDisablesPersistenceAndLeavesExistingFileUntouched() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("recent.json"));
        RecentMediaHistory history;
        QVERIFY(history.load(path).isEmpty());
        history.remember(QStringLiteral("/missing/old.mkv"));
        QVERIFY(history.save().isEmpty());
        const auto before = read(path);
        QVERIFY(history.load({}).isEmpty());
        history.remember(QStringLiteral("/missing/new.mkv"));
        QVERIFY(history.save().isEmpty());
        QCOMPARE(read(path), before);
    }

    void loadingNormalizesDuplicateSourcesAndRetainsFirstOccurrence() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto path = temporary.filePath(QStringLiteral("recent.json"));
        const QByteArray bytes("{\"schemaVersion\":1,\"entries\":[\"/missing/a.mkv\","
                               "\"/missing/b.mkv\",\"file:///missing/a.mkv\"]}");
        QVERIFY(write(path, bytes));
        RecentMediaHistory history;
        QVERIFY(history.load(path).isEmpty());
        QCOMPARE(history.entries(),
                 QStringList({QStringLiteral("/missing/a.mkv"), QStringLiteral("/missing/b.mkv")}));
    }
};

QTEST_GUILESS_MAIN(RecentMediaHistoryTest)
#include "recent_media_history_test.moc"
