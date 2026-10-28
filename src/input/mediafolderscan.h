#pragma once

#include <QStringList>
#include <atomic>
#include <functional>
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

// Testing seam: performs the same scan, but the final natural sort also polls
// `sortCancelled` between bounded merge chunks. Tests use this to cancel
// deterministically while sorting. Production code uses the overload above.
[[nodiscard]] MediaFolderScanResult
scanMediaFolder(const QString &directory, bool recursive,
                const std::shared_ptr<std::atomic_bool> &cancelled,
                const std::function<bool()> &sortCancelled);

[[nodiscard]] MediaFolderScanResult
scanMediaFolders(const QStringList &directories, bool recursive,
                 const std::shared_ptr<std::atomic_bool> &cancelled);
