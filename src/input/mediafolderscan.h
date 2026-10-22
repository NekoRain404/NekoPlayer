#pragma once

#include <QStringList>
#include <atomic>
#include <memory>

struct MediaFolderScanResult {
    QStringList paths;
    QString error;
    bool cancelled = false;
};

// Filesystem work only; safe to call without a QObject or event loop.
[[nodiscard]] MediaFolderScanResult
scanMediaFolder(const QString &directory, bool recursive,
                const std::shared_ptr<std::atomic_bool> &cancelled);
