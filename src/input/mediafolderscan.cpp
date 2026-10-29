#include "mediafolderscan.h"
#include "mediafiletypes.h"
#include "playback/playlist/naturalcompare.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>

namespace {
constexpr qsizetype maximumVisitedEntries = 100000;
// Bounded sort steps keep cancellation latency at milliseconds even for a scan
// at the visited-entry cap instead of one uninterruptible O(n log n) call.
constexpr qsizetype sortChunkSize = 1024;

using CancellationCheck = std::function<bool()>;

std::filesystem::path nativePath(const QString &path) {
#ifdef Q_OS_WIN
    return std::filesystem::path(path.toStdWString());
#else
    return std::filesystem::path(QFile::encodeName(path).toStdString());
#endif
}

QString displayPath(const std::filesystem::path &path) {
#ifdef Q_OS_WIN
    return QString::fromStdWString(path.native());
#else
    return QFile::decodeName(path.native().c_str());
#endif
}

bool supportedMedia(const QFileInfo &file) {
    return supportedMediaExtensions().contains(file.suffix().toLower());
}

MediaFolderScanResult unreadable(const QString &path) {
    return {{}, QStringLiteral("无法读取媒体文件夹：%1").arg(path), false};
}

// Stable merge: the left run wins ties so equal keys keep discovery order,
// matching a whole-list std::stable_sort. Cancellation is polled every
// sortChunkSize emitted elements, so a single large merge stays interruptible.
bool mergeRuns(const QStringList &source, QStringList &target, qsizetype left, qsizetype middle,
               qsizetype right, const CancellationCheck &isCancelled) {
    qsizetype head = left;
    qsizetype tail = middle;
    qsizetype out = left;
    qsizetype sinceCheck = 0;
    while (out < right) {
        if (head < middle && (tail >= right || !naturalLess(source.at(tail), source.at(head))))
            target[out++] = source.at(head++);
        else
            target[out++] = source.at(tail++);
        if (++sinceCheck >= sortChunkSize) {
            if (isCancelled())
                return false;
            sinceCheck = 0;
        }
    }
    return true;
}

// Bottom-up merge sort over initially sorted chunks. The chunk pre-sort plus
// stable merges produce exactly the ordering of std::stable_sort over the whole
// list, while cancellation is observed between chunks instead of only after
// the entire sort. Returns false as soon as cancellation is observed; the
// caller discards `paths` in that case.
[[nodiscard]] bool stableNaturalSort(QStringList &paths, const CancellationCheck &isCancelled) {
    const auto count = paths.size();
    if (count < 2)
        return true;
    for (qsizetype start = 0; start < count; start += sortChunkSize) {
        if (isCancelled())
            return false;
        const auto stop = std::min(start + sortChunkSize, count);
        std::stable_sort(paths.begin() + start, paths.begin() + stop, naturalLess);
    }
    if (count <= sortChunkSize)
        return true;
    QStringList merged;
    merged.resize(count);
    for (qsizetype width = sortChunkSize; width < count; width *= 2) {
        for (qsizetype left = 0; left < count; left += width * 2) {
            if (isCancelled())
                return false;
            const auto middle = std::min(left + width, count);
            const auto right = std::min(left + width * 2, count);
            if (!mergeRuns(paths, merged, left, middle, right, isCancelled))
                return false;
        }
        paths.swap(merged);
    }
    return true;
}
} // namespace

MediaFolderScanResult scanMediaFolder(const QString &directory, bool recursive,
                                      const std::shared_ptr<std::atomic_bool> &cancelled) {
    return scanMediaFolder(directory, recursive, cancelled, {});
}

MediaFolderScanResult scanMediaFolder(const QString &directory, bool recursive,
                                      const std::shared_ptr<std::atomic_bool> &cancelled,
                                      const std::function<bool()> &sortCancelled) {
    const auto isCancelled = [&] {
        return cancelled && cancelled->load(std::memory_order_relaxed);
    };
    if (isCancelled())
        return {{}, {}, true};
    if (directory.isEmpty() || directory.contains(QChar::Null))
        return unreadable(directory);
    // Clean trailing separators before checking the root itself for symlinks.
    const QFileInfo root(QDir::cleanPath(directory));
    if (root.isSymLink() || !root.isDir() || !root.isReadable())
        return unreadable(directory);

    QStringList paths;
    std::vector<std::filesystem::path> pending{nativePath(root.absoluteFilePath())};
    qsizetype visited = 0;
    while (!pending.empty()) {
        if (isCancelled())
            return {{}, {}, true};
        const auto current = std::move(pending.back());
        pending.pop_back();
        std::error_code error;
        auto entry = std::filesystem::directory_iterator(current, error);
        const auto end = std::filesystem::directory_iterator();
        if (error)
            return unreadable(displayPath(current));
        while (entry != end) {
            if (isCancelled())
                return {{}, {}, true};
            if (++visited > maximumVisitedEntries)
                return {{}, QStringLiteral("文件夹内容超过扫描上限（100000 项）"), false};
            const auto status = entry->symlink_status(error);
            if (error)
                return unreadable(displayPath(entry->path()));
            if (!std::filesystem::is_symlink(status)) {
                const QFileInfo file(displayPath(entry->path()));
                if (!file.isHidden()) {
                    if (std::filesystem::is_directory(status) && recursive)
                        pending.push_back(entry->path());
                    else if (std::filesystem::is_regular_file(status) && supportedMedia(file))
                        paths.append(file.absoluteFilePath());
                }
            }
            entry.increment(error);
            if (error)
                return unreadable(displayPath(current));
        }
    }
    if (isCancelled())
        return {{}, {}, true};
    // The shared helper pins an English locale so ordering is natural and
    // deterministic across the user's locale settings and matches title sorts.
    const auto sortCancelledNow = [&] {
        return isCancelled() || (sortCancelled && sortCancelled());
    };
    if (!stableNaturalSort(paths, sortCancelledNow))
        return {{}, {}, true};
    // Keep the pre-existing final poll: a cancellation requested after the
    // last merge chunk must still discard the result rather than commit it.
    if (isCancelled())
        return {{}, {}, true};
    return {std::move(paths), {}, false};
}

MediaFolderScanResult scanMediaFolders(const QStringList &directories, bool recursive,
                                       const std::shared_ptr<std::atomic_bool> &cancelled) {
    if (directories.isEmpty())
        return {{}, QStringLiteral("未指定媒体文件夹"), false};
    QStringList paths;
    for (const auto &directory : directories) {
        const auto result = scanMediaFolder(directory, recursive, cancelled);
        if (result.cancelled || !result.error.isEmpty())
            return result;
        paths.append(result.paths);
    }
    // Keep the user's folder drop order; each root was already naturally
    // sorted by scanMediaFolder. Sorting absolute paths here would make the
    // result depend on temporary/root names rather than the drop gesture.
    return {std::move(paths), {}, false};
}
