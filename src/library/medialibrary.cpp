#include "medialibrary.h"

#include "input/mediafolderscan.h"
#include "playback/media/mediasource.h"
#include "playback/playlist/naturalcompare.h"
#include "session/configfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QSet>
#include <QSettings>
#include <QTemporaryFile>
#include <QtConcurrentRun>
#include <algorithm>
#include <utility>

namespace {
constexpr int SchemaVersion = 1;
constexpr qint64 MaximumConfigurationBytes = 4 * 1024 * 1024;

bool configurationEnabled() {
    const auto *application = QCoreApplication::instance();
    if (!application)
        return true;
    const auto value = application->property("configurationEnabled");
    return !value.isValid() || value.toBool();
}

QString configurationError(const QString &path) {
    return QCoreApplication::translate("MediaLibrary", "无法读写媒体库配置：%1").arg(path);
}

QString schemaError(const QString &path) {
    return QCoreApplication::translate("MediaLibrary",
                                       "媒体库配置格式无效或版本不受支持，已保留原文件：%1")
        .arg(path);
}

QString entryLimitError() {
    return QCoreApplication::translate("MediaLibrary", "媒体库条目超过上限（%1 项），已截断。")
        .arg(MediaLibrary::MaximumEntries);
}

QString openValidatedSettings(const QString &path, QTemporaryFile &snapshot,
                              std::unique_ptr<QSettings> &settings) {
    if (!QFileInfo::exists(path))
        return {};
    const QFileInfo existing(path);
    if (!existing.isFile() || existing.size() > MaximumConfigurationBytes)
        return configurationError(path);
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly) || !snapshot.open())
        return configurationError(path);
    const auto bytes = source.readAll();
    if (source.error() != QFileDevice::NoError || snapshot.write(bytes) != bytes.size() ||
        !snapshot.flush())
        return configurationError(path);
    // Parse a unique snapshot: QSettings caches parses by file name, and a
    // cached format error would otherwise hide a later problem from this read.
    auto parsed = std::make_unique<QSettings>(snapshot.fileName(), QSettings::IniFormat);
    if (parsed->status() != QSettings::NoError)
        return configurationError(path);
    if (parsed->allKeys().isEmpty())
        return {};
    bool valid = false;
    const auto version = parsed->value(QStringLiteral("schemaVersion")).toInt(&valid);
    if (!valid || version != SchemaVersion)
        return schemaError(path);
    settings = std::move(parsed);
    return {};
}

QString validateShape(QSettings &settings, const QString &path) {
    bool valid = false;
    const auto folders = settings.value(QStringLiteral("folders/size"), 0).toInt(&valid);
    if (!valid || folders < 0 || folders > MediaLibrary::MaximumFolders)
        return schemaError(path);
    const auto entries = settings.value(QStringLiteral("entries/size"), 0).toInt(&valid);
    if (!valid || entries < 0 || entries > MediaLibrary::MaximumEntries)
        return schemaError(path);
    return {};
}

bool readBool(QSettings &settings, const QString &key, bool fallback = false) {
    const auto value = settings.value(key).toString().toLower();
    if (value == QLatin1String("true") || value == QLatin1String("1"))
        return true;
    if (value == QLatin1String("false") || value == QLatin1String("0"))
        return false;
    return fallback;
}

QString normalizedAbsolutePath(const QString &path) {
    if (path.isEmpty() || path.size() > MediaLibrary::MaximumPathLength ||
        path.contains(QChar::Null) || !QDir::isAbsolutePath(path))
        return {};
    return QDir::cleanPath(path);
}

QString normalizedFolder(const QString &path) {
    const auto clean = normalizedAbsolutePath(path);
    if (clean.isEmpty())
        return {};
    const QFileInfo info(clean);
    // The folder scanner rejects a symlink root, so the store does too.
    if (!info.isDir() || info.isSymLink())
        return {};
    return clean;
}

QString normalizedEntryPath(const QString &path) {
    const auto clean = normalizedAbsolutePath(path);
    if (clean.isEmpty() || !QFileInfo(clean).isFile())
        return {};
    return clean;
}

QString canonicalKey(const QString &path) {
    const auto canonical = QFileInfo(path).canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(path) : canonical;
}

bool pathInsideFolder(const QString &path, const QString &folder) {
    const auto cleanPath = QDir::cleanPath(path);
    const auto cleanFolder = QDir::cleanPath(folder);
    if (cleanPath == cleanFolder)
        return true;
    const auto prefix =
        cleanFolder.endsWith(QLatin1Char('/')) ? cleanFolder : cleanFolder + QLatin1Char('/');
    return cleanPath.startsWith(prefix);
}

QList<MediaLibraryEntry> indexEntries(const QStringList &paths, bool canonicalDedupe,
                                      QString &error) {
    QSet<QString> seen;
    QStringList unique;
    unique.reserve(paths.size());
    for (const auto &path : paths) {
        const auto clean = normalizedAbsolutePath(path);
        if (clean.isEmpty())
            continue;
        const auto key = canonicalDedupe ? canonicalKey(clean) : clean;
        if (seen.contains(key))
            continue;
        seen.insert(key);
        unique.append(clean);
    }
    std::sort(unique.begin(), unique.end(), naturalLess);
    if (unique.size() > MediaLibrary::MaximumEntries) {
        unique.resize(MediaLibrary::MaximumEntries);
        error = entryLimitError();
    }
    QList<MediaLibraryEntry> entries;
    entries.reserve(unique.size());
    for (const auto &path : unique)
        entries.append({path, MediaSource::displayName(path)});
    return entries;
}
} // namespace

MediaLibrary::MediaLibrary(QString filePath, QObject *parent)
    : QObject(parent), filePath_(std::move(filePath)) {}

MediaLibrary::~MediaLibrary() {
    ++scanGeneration_;
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    cancellation_.reset();
}

const QStringList &MediaLibrary::folders() const { return folders_; }

bool MediaLibrary::addFolder(const QString &absoluteDir) {
    const auto folder = normalizedFolder(absoluteDir);
    if (folder.isEmpty())
        return false;
    const auto key = canonicalKey(folder);
    for (const auto &existing : folders_) {
        if (canonicalKey(existing) == key)
            return true;
    }
    if (folders_.size() >= MaximumFolders)
        return false;
    folders_.append(folder);
    emit foldersChanged();
    return true;
}

bool MediaLibrary::removeFolder(const QString &absoluteDir) {
    const auto folder = normalizedAbsolutePath(absoluteDir);
    if (folder.isEmpty())
        return false;
    const auto key = canonicalKey(folder);
    int found = -1;
    for (int index = 0; index < folders_.size(); ++index) {
        if (QDir::cleanPath(folders_.at(index)) == folder ||
            canonicalKey(folders_.at(index)) == key) {
            found = index;
            break;
        }
    }
    if (found < 0)
        return false;
    folders_.removeAt(found);
    QList<MediaLibraryEntry> retained;
    retained.reserve(entries_.size());
    for (const auto &entry : entries_) {
        const bool watched =
            std::any_of(folders_.cbegin(), folders_.cend(), [&entry](const auto &remaining) {
                return pathInsideFolder(entry.path, remaining);
            });
        if (watched)
            retained.append(entry);
    }
    const bool entriesRemoved = retained.size() != entries_.size();
    entries_ = std::move(retained);
    emit foldersChanged();
    if (entriesRemoved)
        emit entriesChanged();
    return true;
}

const QList<MediaLibraryEntry> &MediaLibrary::entries() const { return entries_; }

bool MediaLibrary::recursive() const { return recursive_; }

void MediaLibrary::setRecursive(bool recursive) {
    if (recursive_ == recursive)
        return;
    recursive_ = recursive;
    emit recursiveChanged(recursive_);
}

void MediaLibrary::clearEntries() {
    if (entries_.isEmpty())
        return;
    entries_.clear();
    emit entriesChanged();
}

QString MediaLibrary::load() {
    if (filePath_.isEmpty() || !configurationEnabled())
        return {};
    QTemporaryFile snapshot;
    std::unique_ptr<QSettings> parsed;
    if (const auto error = openValidatedSettings(filePath_, snapshot, parsed); !error.isEmpty())
        return error;
    if (!parsed) {
        const bool hadData = !folders_.isEmpty() || !entries_.isEmpty() || recursive_;
        folders_.clear();
        entries_.clear();
        recursive_ = false;
        if (hadData) {
            emit foldersChanged();
            emit entriesChanged();
            emit recursiveChanged(false);
        }
        return {};
    }
    auto &settings = *parsed;
    if (const auto error = validateShape(settings, filePath_); !error.isEmpty())
        return error;

    const auto folderCount = settings.value(QStringLiteral("folders/size")).toInt();
    QStringList folders;
    QSet<QString> folderKeys;
    for (int index = 0; index < folderCount; ++index) {
        const auto folder =
            normalizedFolder(settings.value(QStringLiteral("folders/%1").arg(index)).toString());
        if (folder.isEmpty())
            continue;
        const auto key = canonicalKey(folder);
        if (folderKeys.contains(key))
            continue;
        folderKeys.insert(key);
        folders.append(folder);
    }

    const auto entryCount = settings.value(QStringLiteral("entries/size")).toInt();
    QStringList paths;
    paths.reserve(entryCount);
    for (int index = 0; index < entryCount; ++index) {
        const auto path = normalizedEntryPath(
            settings.value(QStringLiteral("entries/%1/path").arg(index)).toString());
        if (!path.isEmpty())
            paths.append(path);
    }
    QString indexError;
    auto entries = indexEntries(paths, false, indexError);
    const bool recursive = readBool(settings, QStringLiteral("recursive"));
    folders_ = std::move(folders);
    entries_ = std::move(entries);
    recursive_ = recursive;
    emit foldersChanged();
    emit entriesChanged();
    emit recursiveChanged(recursive_);
    return {};
}

QString MediaLibrary::save() {
    if (filePath_.isEmpty() || !configurationEnabled())
        return {};
    QTemporaryFile snapshot;
    std::unique_ptr<QSettings> parsed;
    if (const auto error = openValidatedSettings(filePath_, snapshot, parsed); !error.isEmpty())
        return error;
    if (parsed) {
        if (const auto error = validateShape(*parsed, filePath_); !error.isEmpty())
            return error;
    }

    QStringList folders;
    QSet<QString> folderKeys;
    for (const auto &folder : folders_) {
        const auto normalized = normalizedFolder(folder);
        if (normalized.isEmpty())
            continue;
        const auto key = canonicalKey(normalized);
        if (folderKeys.contains(key))
            continue;
        folderKeys.insert(key);
        folders.append(normalized);
        if (folders.size() >= MaximumFolders)
            break;
    }
    QStringList paths;
    paths.reserve(entries_.size());
    for (const auto &entry : entries_) {
        const auto path = normalizedEntryPath(entry.path);
        if (!path.isEmpty())
            paths.append(path);
    }
    QString indexError;
    const auto entries = indexEntries(paths, false, indexError);

    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath()))
        return configurationError(filePath_);
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setAtomicSyncRequired(true);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    settings.clear();
    settings.setValue(QStringLiteral("schemaVersion"), SchemaVersion);
    settings.setValue(QStringLiteral("recursive"), recursive_);
    settings.setValue(QStringLiteral("folders/size"), folders.size());
    for (qsizetype index = 0; index < folders.size(); ++index)
        settings.setValue(QStringLiteral("folders/%1").arg(index), folders.at(index));
    settings.setValue(QStringLiteral("entries/size"), entries.size());
    for (qsizetype index = 0; index < entries.size(); ++index)
        settings.setValue(QStringLiteral("entries/%1/path").arg(index), entries.at(index).path);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    restrictConfigurationFilePermissions(filePath_);
    return {};
}

bool MediaLibrary::isScanning() const { return scanning_; }

void MediaLibrary::startScan() {
    if (scanning_)
        cancelScan();
    const auto generation = ++scanGeneration_;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    scanning_ = true;
    scanIndex_ = 0;
    scanOverflow_ = false;
    scanFolders_ = folders_;
    scanPaths_.clear();
    scanErrors_.clear();
    emit scanStarted(static_cast<int>(scanFolders_.size()));
    if (!scanning_ || generation != scanGeneration_)
        return;
    emit scanProgress(0, static_cast<int>(scanFolders_.size()));
    if (!scanning_ || generation != scanGeneration_)
        return;
    if (scanFolders_.isEmpty()) {
        finishScan(tr("尚未添加媒体文件夹。"));
        return;
    }
    scanNextFolder(generation);
}

void MediaLibrary::cancelScan() {
    ++scanGeneration_;
    if (cancellation_)
        cancellation_->store(true, std::memory_order_relaxed);
    cancellation_.reset();
    scanFolders_.clear();
    scanPaths_.clear();
    scanErrors_.clear();
    scanIndex_ = 0;
    scanOverflow_ = false;
    if (!scanning_)
        return;
    scanning_ = false;
    emit scanFinished(true, {});
}

void MediaLibrary::scanNextFolder(quint64 generation) {
    if (!scanning_ || generation != scanGeneration_)
        return;
    if (scanIndex_ >= scanFolders_.size()) {
        finishScan({});
        return;
    }
    const auto folder = scanFolders_.at(scanIndex_);
    const auto recursive = recursive_;
    const auto cancellation = cancellation_;
    auto *watcher = new QFutureWatcher<MediaFolderScanResult>(this);
    connect(watcher, &QFutureWatcher<MediaFolderScanResult>::finished, this,
            [this, watcher, generation] {
                const auto result = watcher->result();
                watcher->deleteLater();
                if (!scanning_ || generation != scanGeneration_)
                    return;
                if (result.cancelled) {
                    cancelScan();
                    return;
                }
                if (!result.error.isEmpty()) {
                    scanErrors_.append(result.error);
                } else {
                    for (const auto &path : result.paths) {
                        if (scanPaths_.size() >= MaximumEntries) {
                            scanOverflow_ = true;
                            break;
                        }
                        scanPaths_.append(path);
                    }
                }
                ++scanIndex_;
                emit scanProgress(scanIndex_, static_cast<int>(scanFolders_.size()));
                if (!scanning_ || generation != scanGeneration_)
                    return;
                if (scanOverflow_) {
                    finishScan(entryLimitError());
                    return;
                }
                scanNextFolder(generation);
            });
    watcher->setFuture(QtConcurrent::run([folder, recursive, cancellation] {
        return scanMediaFolder(folder, recursive, cancellation);
    }));
}

void MediaLibrary::finishScan(const QString &error) {
    ++scanGeneration_;
    scanning_ = false;
    cancellation_.reset();
    const auto paths = std::exchange(scanPaths_, {});
    auto errors = std::exchange(scanErrors_, {});
    scanFolders_.clear();
    scanIndex_ = 0;
    scanOverflow_ = false;
    QString indexError;
    entries_ = indexEntries(paths, true, indexError);
    emit entriesChanged();
    if (!error.isEmpty())
        errors.append(error);
    if (!indexError.isEmpty())
        errors.append(indexError);
    emit scanFinished(false, errors.join(QLatin1Char('\n')));
}
