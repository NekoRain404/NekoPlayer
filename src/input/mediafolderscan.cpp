#include "mediafolderscan.h"
#include "mediafiletypes.h"
#include "playback/naturalcompare.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <filesystem>
#include <vector>

namespace {
constexpr qsizetype maximumVisitedEntries = 100000;

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
} // namespace

MediaFolderScanResult scanMediaFolder(const QString &directory, bool recursive,
                                      const std::shared_ptr<std::atomic_bool> &cancelled) {
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
    std::sort(paths.begin(), paths.end(), naturalLess);
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
