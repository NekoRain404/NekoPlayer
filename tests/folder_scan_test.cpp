#include "input/folderscanjob.h"
#include "input/mediafolderscan.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QRegularExpression>
#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <QtTest>
#include <memory>

namespace {
bool touch(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::WriteOnly);
}

MediaFolderScanResult scan(const QString &path, bool recursive = false) {
    return scanMediaFolder(path, recursive, std::make_shared<std::atomic_bool>(false));
}

// Hold the real global pool so queued cancellation/replacement is deterministic.
class PoolBlocker final {
  public:
    PoolBlocker() : previousMaximum_(QThreadPool::globalInstance()->maxThreadCount()) {
        QThreadPool::globalInstance()->waitForDone();
        QThreadPool::globalInstance()->setMaxThreadCount(1);
        future_ = QtConcurrent::run([this] {
            entered_.release();
            release_.acquire();
        });
        started_ = entered_.tryAcquire(1, 5000);
    }
    ~PoolBlocker() {
        unblock();
        QThreadPool::globalInstance()->waitForDone();
        QThreadPool::globalInstance()->setMaxThreadCount(previousMaximum_);
    }
    bool started() const { return started_; }
    void unblock() {
        if (!released_) {
            released_ = true;
            release_.release();
        }
    }

  private:
    QSemaphore entered_;
    QSemaphore release_;
    QFuture<void> future_;
    int previousMaximum_;
    bool started_ = false;
    bool released_ = false;
};
} // namespace

class FolderScanTest final : public QObject {
    Q_OBJECT
  private slots:
    void filtersAndNaturallySortsDirectChildren() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (const auto *name : {"episode10.MKV", "episode2.mp4", "episode1.flac", "cover.jpg",
                                 "captions.srt", "queue.m3u8", "readme", ".hidden.mp3"})
            QVERIFY(touch(directory.filePath(QString::fromLatin1(name))));
        QVERIFY(QDir(directory.path()).mkdir("nested"));
        QVERIFY(touch(directory.filePath("nested/episode0.mp4")));
        const auto result = scan(directory.path());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.paths, QStringList({directory.filePath("episode1.flac"),
                                            directory.filePath("episode2.mp4"),
                                            directory.filePath("episode10.MKV")}));
        QVERIFY(!result.cancelled);
    }

    void includesPracticalVideoAndAudioExtensions() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QStringList suffixes =
            QStringLiteral("mkv mp4 avi mov webm m4v ts m2ts mts mpg mpeg wmv flv ogv 3gp vob "
                           "mp3 flac wav m4a aac ogg opus wma aiff aif mka ac3 dts ape")
                .split(' ');
        for (const auto &suffix : suffixes)
            QVERIFY(touch(directory.filePath("media." + suffix.toUpper())));
        const auto result = scan(directory.path());
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.paths.size(), suffixes.size());
    }

    void recursiveScanSkipsHiddenDirectoriesAndAllSymlinks() {
        QTemporaryDir directory;
        QTemporaryDir outside;
        QVERIFY(directory.isValid() && outside.isValid());
        QVERIFY(QDir(directory.path()).mkpath("part2/deeper"));
        QVERIFY(QDir(directory.path()).mkdir(".hidden"));
        QVERIFY(touch(directory.filePath("part2/deeper/猫 10.mp4")));
        QVERIFY(touch(directory.filePath("part2/猫 2.mp4")));
        QVERIFY(touch(directory.filePath(".hidden/secret.mp4")));
        QVERIFY(touch(outside.filePath("outside.mp4")));
        QVERIFY(QFile::link(outside.filePath("outside.mp4"), directory.filePath("linked.mp4")));
        QVERIFY(QFile::link(outside.path(), directory.filePath("linked-directory")));
        QVERIFY(QFile::link(directory.path(), directory.filePath("part2/loop")));
        const auto result = scan(directory.path(), true);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QCOMPARE(result.paths, QStringList({directory.filePath("part2/deeper/猫 10.mp4"),
                                            directory.filePath("part2/猫 2.mp4")}));
        QVERIFY(!scan(directory.filePath("linked-directory"), true).error.isEmpty());
        QVERIFY(!scan(directory.filePath("linked-directory/"), true).error.isEmpty());
    }

    void rejectsInvalidRootsAndAcceptsAnEmptyFolder() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("media.mp4")));
        QVERIFY(!scan(QString()).error.isEmpty());
        QVERIFY(!scan(directory.filePath("missing")).error.isEmpty());
        QVERIFY(!scan(directory.filePath("media.mp4")).error.isEmpty());
        QVERIFY(QDir(directory.path()).mkdir("empty"));
        const auto result = scan(directory.filePath("empty"));
        QVERIFY(result.error.isEmpty());
        QVERIFY(result.paths.isEmpty());
    }

    void rejectsUnreadableRoot() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto permissions = QFile::permissions(directory.path());
        QVERIFY(QFile::setPermissions(directory.path(), {}));
        const auto readable = QFileInfo(directory.path()).isReadable();
        const auto result = scan(directory.path());
        QVERIFY(QFile::setPermissions(directory.path(), permissions));
        if (readable)
            QSKIP("Current account can read directories without permission bits");
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.paths.isEmpty());
    }

    void rejectsEmbeddedNullWithoutScanningPrefix() {
        QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("media.mp4")));
        const auto result = scan(directory.path() + QChar::Null + QStringLiteral("ignored"));
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.paths.isEmpty());
    }

    void countsUnknownFilesAndDirectoriesTowardsTraversalLimit() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int i = 0; i < 99999; ++i)
            QVERIFY(touch(directory.filePath(QString::number(i) + ".unknown")));
        QVERIFY(QDir(directory.path()).mkdir("nested"));
        auto result = scan(directory.path(), true);
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.paths.isEmpty());
        QVERIFY(touch(directory.filePath("nested/media.mp4")));
        result = scan(directory.path(), true);
        QVERIFY(!result.error.isEmpty());
        QVERIFY(result.paths.isEmpty());
    }

    void cancelledWorkerDoesNotPublishPaths() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("media.mp4")));
        const auto result =
            scanMediaFolder(directory.path(), true, std::make_shared<std::atomic_bool>(true));
        QVERIFY(result.cancelled);
        QVERIFY(result.paths.isEmpty());
        QVERIFY(result.error.isEmpty());
    }

    void completesOnAppThreadAfterClearingBusy() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("media.mp4")));
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        QSignalSpy changed(&job, &FolderScanJob::scanningChanged);
        QSignalSpy failed(&job, &FolderScanJob::failed);
        bool correctThread = false;
        bool idleDuringCompletion = false;
        connect(&job, &FolderScanJob::completed, this, [&] {
            correctThread = QThread::currentThread() == thread();
            idleDuringCompletion = !job.isScanning();
        });
        job.start(directory.path());
        QVERIFY(job.isScanning());
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.first().first().toStringList(),
                 QStringList({directory.filePath("media.mp4")}));
        QCOMPARE(changed.count(), 2);
        QVERIFY(correctThread && idleDuringCompletion);
        QVERIFY(failed.isEmpty());
    }

    void reportsFailureAfterClearingBusy() {
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        QSignalSpy failed(&job, &FolderScanJob::failed);
        bool idleDuringFailure = false;
        connect(&job, &FolderScanJob::failed, this, [&] { idleDuringFailure = !job.isScanning(); });
        job.start("/this-folder-must-not-exist/nekoplayer");
        QTRY_COMPARE(failed.count(), 1);
        QVERIFY(!failed.first().first().toString().isEmpty());
        QVERIFY(idleDuringFailure);
        QVERIFY(completed.isEmpty());
    }

    void immediateCancelSuppressesResultsAndErrors() {
        PoolBlocker blocker;
        QVERIFY(blocker.started());
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        QSignalSpy failed(&job, &FolderScanJob::failed);
        QSignalSpy changed(&job, &FolderScanJob::scanningChanged);
        job.start("/missing-folder");
        QVERIFY(job.isScanning());
        job.cancel();
        QVERIFY(!job.isScanning());
        QCOMPARE(changed.count(), 2);
        blocker.unblock();
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QVERIFY(completed.isEmpty() && failed.isEmpty());
    }

    void replacementSuppressesOldResults() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("new.mp4")));
        PoolBlocker blocker;
        QVERIFY(blocker.started());
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        QSignalSpy failed(&job, &FolderScanJob::failed);
        job.start("/missing-folder");
        job.start(directory.path());
        blocker.unblock();
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.first().first().toStringList(),
                 QStringList({directory.filePath("new.mp4")}));
        QVERIFY(failed.isEmpty());
    }

    void deletionDoesNotWaitForQueuedWorker() {
        PoolBlocker blocker;
        QVERIFY(blocker.started());
        int notifications = 0;
        auto job = std::make_unique<FolderScanJob>();
        connect(job.get(), &FolderScanJob::completed, this, [&] { ++notifications; });
        connect(job.get(), &FolderScanJob::failed, this, [&] { ++notifications; });
        job->start("/missing-folder");
        QElapsedTimer elapsed;
        elapsed.start();
        job.reset();
        QVERIFY(elapsed.elapsed() < 1000);
        blocker.unblock();
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(notifications, 0);
    }

    void deletionSuppressesLargeScanCompletion() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        for (int i = 0; i < 30000; ++i)
            QVERIFY(touch(directory.filePath(QString::number(i) + ".mp4")));
        QThreadPool::globalInstance()->waitForDone();
        int notifications = 0;
        auto job = std::make_unique<FolderScanJob>();
        connect(job.get(), &FolderScanJob::completed, this, [&] { ++notifications; });
        connect(job.get(), &FolderScanJob::failed, this, [&] { ++notifications; });
        job->start(directory.path());
        // Destroy before delivering completion. Depending on scheduling, the
        // real worker may be queued, running, or awaiting result delivery.
        QElapsedTimer elapsed;
        elapsed.start();
        job.reset();
        QVERIFY(elapsed.elapsed() < 1000);
        QVERIFY(QThreadPool::globalInstance()->waitForDone(5000));
        QCoreApplication::processEvents();
        QCOMPARE(notifications, 0);
    }

    void deletionFromBusySignalIsSafe() {
        auto job = std::make_unique<FolderScanJob>();
        connect(job.get(), &FolderScanJob::scanningChanged, this, [&] { job.reset(); });
        job->start("/missing-folder");
        QVERIFY(!job);
    }

    void deletionFromIdleSignalIsSafe() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        int notifications = 0;
        auto job = std::make_unique<FolderScanJob>();
        connect(job.get(), &FolderScanJob::completed, this, [&] { ++notifications; });
        connect(job.get(), &FolderScanJob::scanningChanged, this, [&] {
            if (!job->isScanning())
                job.reset();
        });
        job->start(directory.path());
        QTRY_VERIFY(!job);
        QCOMPARE(notifications, 0);
    }

    void newerStartFromBusySignalWins() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QVERIFY(touch(directory.filePath("new.mp4")));
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        QSignalSpy failed(&job, &FolderScanJob::failed);
        bool replaced = false;
        connect(&job, &FolderScanJob::scanningChanged, this, [&] {
            if (job.isScanning() && !replaced) {
                replaced = true;
                job.start(directory.path());
            }
        });
        job.start("/missing-folder");
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.first().first().toStringList(),
                 QStringList({directory.filePath("new.mp4")}));
        QVERIFY(failed.isEmpty());
    }

    void newerStartFromIdleSignalSuppressesStaleCompletion() {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid() && second.isValid());
        QVERIFY(touch(first.filePath("old.mp4")));
        QVERIFY(touch(second.filePath("new.mp4")));
        FolderScanJob job;
        QSignalSpy completed(&job, &FolderScanJob::completed);
        bool replaced = false;
        connect(&job, &FolderScanJob::scanningChanged, this, [&] {
            if (!job.isScanning() && !replaced) {
                replaced = true;
                job.start(second.path());
            }
        });
        job.start(first.path());
        QTRY_COMPARE(completed.count(), 1);
        QCOMPARE(completed.first().first().toStringList(),
                 QStringList({second.filePath("new.mp4")}));
    }
};

QTEST_GUILESS_MAIN(FolderScanTest)
#include "folder_scan_test.moc"
