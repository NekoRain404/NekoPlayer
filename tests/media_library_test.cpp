#include "library/medialibrary.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {
bool touch(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

bool scanAndWait(MediaLibrary &library) {
    QSignalSpy finished(&library, &MediaLibrary::scanFinished);
    library.startScan();
    return finished.wait(10000);
}

QByteArray readAll(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
} // namespace

class MediaLibraryTest final : public QObject {
    Q_OBJECT
  private:
    QVariant previousConfiguration_;

  private slots:
    void init() {
        previousConfiguration_ = qApp->property("configurationEnabled");
        qApp->setProperty("configurationEnabled", true);
    }

    void cleanup() {
        if (previousConfiguration_.isValid())
            qApp->setProperty("configurationEnabled", previousConfiguration_);
        else
            qApp->setProperty("configurationEnabled", QVariant{});
    }

    void storeRoundTripPersistsFoldersEntriesAndRecursive() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("media")));
        const auto folder = temporary.filePath(QStringLiteral("media"));
        QVERIFY(touch(QDir(folder).filePath(QStringLiteral("episode 10.mkv"))));
        QVERIFY(touch(QDir(folder).filePath(QStringLiteral("episode 2.mp4"))));
        const auto path = temporary.filePath(QStringLiteral("library.ini"));
        {
            MediaLibrary library(path);
            QVERIFY(library.addFolder(folder));
            library.setRecursive(true);
            QVERIFY(scanAndWait(library));
            QCOMPARE(library.entries().size(), 2);
            QVERIFY(library.save().isEmpty());
        }

        MediaLibrary loaded(path);
        QVERIFY2(loaded.load().isEmpty(), "a saved library must load cleanly");
        QCOMPARE(loaded.folders(), QStringList{folder});
        QVERIFY(loaded.recursive());
        QCOMPARE(loaded.entries().size(), 2);
        QCOMPARE(loaded.entries().at(0).displayName, QStringLiteral("episode 2.mp4"));
        QCOMPARE(loaded.entries().at(1).displayName, QStringLiteral("episode 10.mkv"));
        QCOMPARE(loaded.entries().at(0).path,
                 QDir(folder).filePath(QStringLiteral("episode 2.mp4")));

        const auto permissions = QFile::permissions(path);
        QVERIFY(permissions.testFlag(QFileDevice::ReadOwner));
        QVERIFY(permissions.testFlag(QFileDevice::WriteOwner));
        QVERIFY(!permissions.testFlag(QFileDevice::ReadGroup));
        QVERIFY(!permissions.testFlag(QFileDevice::WriteGroup));
        QVERIFY(!permissions.testFlag(QFileDevice::ReadOther));
        QVERIFY(!permissions.testFlag(QFileDevice::WriteOther));
    }

    void rejectsRelativeMissingAndExcessFolders() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        MediaLibrary library(temporary.filePath(QStringLiteral("library.ini")));
        QVERIFY(!library.addFolder(QStringLiteral("relative/folder")));
        QVERIFY(!library.addFolder(temporary.filePath(QStringLiteral("missing"))));
        QVERIFY(!library.addFolder(
            temporary.filePath(QString(MediaLibrary::MaximumPathLength + 1, QLatin1Char('a')))));

        QString firstFolder;
        for (int index = 0; index < MediaLibrary::MaximumFolders; ++index) {
            const auto name = QStringLiteral("folder %1").arg(index);
            QVERIFY(QDir(temporary.path()).mkdir(name));
            const auto folder = temporary.filePath(name);
            QVERIFY2(library.addFolder(folder), qPrintable(folder));
            if (index == 0)
                firstFolder = folder;
        }
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("overflow")));
        QVERIFY(!library.addFolder(temporary.filePath(QStringLiteral("overflow"))));
        QCOMPARE(library.folders().size(), MediaLibrary::MaximumFolders);
        QVERIFY(library.addFolder(firstFolder));
        QCOMPARE(library.folders().size(), MediaLibrary::MaximumFolders);
    }

    void loadSkipsMissingEntriesAndInvalidFolders() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("watched")));
        const auto watched = temporary.filePath(QStringLiteral("watched"));
        const auto existing = temporary.filePath(QStringLiteral("watched/present.mkv"));
        QVERIFY(touch(existing));
        const auto path = temporary.filePath(QStringLiteral("library.ini"));
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 1);
            settings.setValue(QStringLiteral("recursive"), false);
            settings.setValue(QStringLiteral("folders/size"), 3);
            settings.setValue(QStringLiteral("folders/0"), watched);
            settings.setValue(QStringLiteral("folders/1"), QStringLiteral("relative/folder"));
            settings.setValue(QStringLiteral("folders/2"),
                              temporary.filePath(QStringLiteral("gone")));
            settings.setValue(QStringLiteral("entries/size"), 3);
            settings.setValue(QStringLiteral("entries/0/path"), existing);
            settings.setValue(QStringLiteral("entries/1/path"),
                              temporary.filePath(QStringLiteral("missing.mkv")));
            settings.setValue(
                QStringLiteral("entries/2/path"),
                temporary.filePath(QString(MediaLibrary::MaximumPathLength + 1, QLatin1Char('b'))));
            settings.sync();
        }

        MediaLibrary library(path);
        QVERIFY2(library.load().isEmpty(), "missing entries must be non-fatal");
        QCOMPARE(library.folders(), QStringList{watched});
        QCOMPARE(library.entries().size(), 1);
        QCOMPARE(library.entries().front().path, existing);
    }

    void unknownSchemaAndMalformedFilesArePreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const auto unknownPath = temporary.filePath(QStringLiteral("future.ini"));
        {
            QSettings settings(unknownPath, QSettings::IniFormat);
            settings.setValue(QStringLiteral("schemaVersion"), 2);
            settings.setValue(QStringLiteral("future/value"), QStringLiteral("keep"));
            settings.sync();
        }
        const auto unknown = readAll(unknownPath);
        MediaLibrary future(unknownPath);
        QVERIFY(!future.load().isEmpty());
        QVERIFY(!future.save().isEmpty());
        QCOMPARE(readAll(unknownPath), unknown);

        const auto malformedPath = temporary.filePath(QStringLiteral("broken.ini"));
        const QByteArray corrupt("[General]\nschemaVersion=1\n[broken group\nvalue=keep\n");
        QFile malformed(malformedPath);
        QVERIFY(malformed.open(QIODevice::WriteOnly));
        QCOMPARE(malformed.write(corrupt), corrupt.size());
        malformed.close();
        MediaLibrary broken(malformedPath);
        QVERIFY(!broken.load().isEmpty());
        QVERIFY(!broken.save().isEmpty());
        QCOMPARE(readAll(malformedPath), corrupt);
    }

    void countsBeyondCapsAreRejectedAndPreserved() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        int fileNumber = 0;
        for (const auto &[key, value] :
             {std::pair{QStringLiteral("folders/size"), MediaLibrary::MaximumFolders + 1},
              std::pair{QStringLiteral("entries/size"), MediaLibrary::MaximumEntries + 1}}) {
            const auto path =
                temporary.filePath(QStringLiteral("oversized-%1.ini").arg(fileNumber++));
            {
                QSettings settings(path, QSettings::IniFormat);
                settings.setValue(QStringLiteral("schemaVersion"), 1);
                settings.setValue(key, value);
                settings.sync();
            }
            const auto original = readAll(path);
            MediaLibrary library(path);
            QVERIFY(!library.load().isEmpty());
            QVERIFY(!library.save().isEmpty());
            QCOMPARE(readAll(path), original);
        }
    }

    void disabledConfigurationNeverTouchesDisk() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("media")));
        const auto folder = temporary.filePath(QStringLiteral("media"));
        QVERIFY(touch(QDir(folder).filePath(QStringLiteral("in-memory.mkv"))));
        const auto path = temporary.filePath(QStringLiteral("library.ini"));

        qApp->setProperty("configurationEnabled", false);
        MediaLibrary library(path);
        QVERIFY(library.addFolder(folder));
        QVERIFY(scanAndWait(library));
        QCOMPARE(library.entries().size(), 1);
        QVERIFY(library.save().isEmpty());
        QVERIFY(!QFileInfo::exists(path));

        MediaLibrary reloaded(path);
        QVERIFY(reloaded.load().isEmpty());
        QVERIFY(reloaded.folders().isEmpty());
        QVERIFY(reloaded.entries().isEmpty());
    }

    void scanMergesFoldersDeduplicatesAndSortsNaturally() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("root")));
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("root/nested")));
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("root/sub")));
        const auto root = temporary.filePath(QStringLiteral("root"));
        const auto sub = temporary.filePath(QStringLiteral("root/sub"));
        QVERIFY(touch(QDir(root).filePath(QStringLiteral("top.mkv"))));
        QVERIFY(touch(QDir(root).filePath(QStringLiteral("notes.txt"))));
        QVERIFY(touch(QDir(root).filePath(QStringLiteral("nested/deep.mp3"))));
        QVERIFY(touch(QDir(sub).filePath(QStringLiteral("child.mp4"))));

        MediaLibrary library(temporary.filePath(QStringLiteral("library.ini")));
        QVERIFY(library.addFolder(root));
        QVERIFY(library.addFolder(sub));
        library.setRecursive(true);
        QSignalSpy finished(&library, &MediaLibrary::scanFinished);
        library.startScan();
        QVERIFY(finished.wait(10000));
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.at(0).at(0).toBool());
        QVERIFY2(finished.at(0).at(1).toString().isEmpty(),
                 qPrintable(finished.at(0).at(1).toString()));
        QCOMPARE(library.entries().size(), 3);
        QStringList paths;
        for (const auto &entry : library.entries())
            paths.append(entry.path);
        QCOMPARE(paths, QStringList({QDir(root).filePath(QStringLiteral("nested/deep.mp3")),
                                     QDir(sub).filePath(QStringLiteral("child.mp4")),
                                     QDir(root).filePath(QStringLiteral("top.mkv"))}));

        library.setRecursive(false);
        QVERIFY(scanAndWait(library));
        QCOMPARE(library.entries().size(), 2);
        paths.clear();
        for (const auto &entry : library.entries())
            paths.append(entry.path);
        QCOMPARE(paths, QStringList({QDir(sub).filePath(QStringLiteral("child.mp4")),
                                     QDir(root).filePath(QStringLiteral("top.mkv"))}));
    }

    void cancelledScanCommitsNoPartialEntries() {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        QVERIFY(QDir(temporary.path()).mkdir(QStringLiteral("media")));
        const auto folder = temporary.filePath(QStringLiteral("media"));
        QVERIFY(touch(QDir(folder).filePath(QStringLiteral("one.mkv"))));
        MediaLibrary library(temporary.filePath(QStringLiteral("library.ini")));
        QVERIFY(library.addFolder(folder));

        QSignalSpy finished(&library, &MediaLibrary::scanFinished);
        library.startScan();
        library.cancelScan();
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.at(0).at(0).toBool());
        QTest::qWait(100);
        QVERIFY2(library.entries().isEmpty(), "cancellation must not publish partial entries");

        QVERIFY(scanAndWait(library));
        QCOMPARE(library.entries().size(), 1);
        library.startScan();
        library.cancelScan();
        QTest::qWait(100);
        QCOMPARE(library.entries().size(), 1);
        QCOMPARE(library.entries().front().path, QDir(folder).filePath(QStringLiteral("one.mkv")));
    }
};

QTEST_GUILESS_MAIN(MediaLibraryTest)
#include "media_library_test.moc"
