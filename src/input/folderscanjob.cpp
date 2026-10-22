#include "folderscanjob.h"
#include "mediafolderscan.h"

#include <QFutureWatcher>
#include <QPointer>
#include <QtConcurrentRun>

FolderScanJob::FolderScanJob(QObject *parent) : QObject(parent) {}

FolderScanJob::~FolderScanJob() {
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
}

bool FolderScanJob::isScanning() const { return scanning_; }

void FolderScanJob::start(const QString &directory, bool recursive) {
    start(QStringList{directory}, recursive);
}

void FolderScanJob::start(const QStringList &directories, bool recursive) {
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    const auto generation = ++generation_;
    const auto cancellation = std::make_shared<std::atomic_bool>(false);
    cancellation_ = cancellation;
    const auto wasScanning = scanning_;
    scanning_ = true;
    const QPointer<FolderScanJob> guard(this);
    if (!wasScanning)
        emit scanningChanged();
    // Direct signal handlers may cancel, replace, or delete this job.
    if (!guard || generation != generation_)
        return;

    auto *watcher = new QFutureWatcher<MediaFolderScanResult>(this);
    connect(watcher, &QFutureWatcher<MediaFolderScanResult>::finished, this,
            [this, watcher, generation] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (generation != generation_)
                    return;
                cancellation_.reset();
                scanning_ = false;
                const QPointer<FolderScanJob> guard(this);
                emit scanningChanged();
                if (!guard || generation != generation_ || result.cancelled)
                    return;
                if (!result.error.isEmpty())
                    emit failed(result.error);
                else
                    emit completed(result.paths);
            });
    watcher->setFuture(QtConcurrent::run([directories, recursive, cancellation] {
        return scanMediaFolders(directories, recursive, cancellation);
    }));
}

void FolderScanJob::cancel() {
    ++generation_;
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    cancellation_.reset();
    if (!scanning_)
        return;
    scanning_ = false;
    emit scanningChanged();
}
