#include "trackpreferencestore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryFile>
#include <algorithm>
#include <utility>

namespace {
constexpr int SchemaVersion = 1;

QString configurationError(const QString &path) {
    return QCoreApplication::translate("TrackPreferenceStore", "无法读写配置文件：%1").arg(path);
}

QString schemaError(const QString &path) {
    return QCoreApplication::translate("TrackPreferenceStore",
                                       "媒体轨道偏好格式不受支持，已保留原文件：%1")
        .arg(path);
}

QString normalizedSubtitlePath(const QString &path) {
    if (path.isEmpty() || path.size() > TrackPreferenceStore::MaximumPathLength ||
        path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
        return {};
    const QFileInfo info(path);
    const auto canonical = info.exists() ? info.canonicalFilePath() : QString{};
    return QDir::cleanPath(canonical.isEmpty() ? path : canonical);
}

QString validateExistingFile(const QString &path) {
    if (!QFileInfo::exists(path))
        return {};
    QFile source(path);
    QTemporaryFile snapshot;
    if (!source.open(QIODevice::ReadOnly) || !snapshot.open())
        return configurationError(path);
    const auto bytes = source.readAll();
    if (source.error() != QFileDevice::NoError || snapshot.write(bytes) != bytes.size() ||
        !snapshot.flush())
        return configurationError(path);
    QSettings settings(snapshot.fileName(), QSettings::IniFormat);
    const auto keys = settings.allKeys();
    if (settings.status() != QSettings::NoError)
        return configurationError(path);
    if (keys.isEmpty())
        return {};
    bool valid = false;
    const auto version = settings.value(QStringLiteral("schemaVersion")).toInt(&valid);
    return valid && version == SchemaVersion ? QString{} : schemaError(path);
}

bool validOptionalId(const std::optional<qint64> &id) { return !id || *id >= 0; }

bool validEntry(const TrackPreferenceEntry &entry) {
    if (TrackPreferenceStore::normalizedPath(entry.mediaPath).isEmpty() || entry.fileSize < 0 ||
        entry.modifiedUtcMs <= 0 || entry.lastUsedUtcMs <= 0 ||
        !validOptionalId(entry.audioTrackId) || !validOptionalId(entry.subtitleTrackId) ||
        entry.selectedExternalSubtitleStream < -1 || entry.externalSubtitlePaths.size() > 64)
        return false;
    if (!entry.selectedExternalSubtitlePath.isEmpty() &&
        normalizedSubtitlePath(entry.selectedExternalSubtitlePath).isEmpty())
        return false;
    for (const auto &path : entry.externalSubtitlePaths) {
        if (normalizedSubtitlePath(path).isEmpty())
            return false;
    }
    return true;
}

QList<TrackPreferenceEntry> normalizedEntries(const QList<TrackPreferenceEntry> &entries) {
    QList<TrackPreferenceEntry> unique;
    unique.reserve(entries.size());
    for (auto entry : entries) {
        entry.mediaPath = TrackPreferenceStore::normalizedPath(entry.mediaPath);
        for (auto &path : entry.externalSubtitlePaths)
            path = normalizedSubtitlePath(path);
        if (!entry.selectedExternalSubtitlePath.isEmpty())
            entry.selectedExternalSubtitlePath =
                normalizedSubtitlePath(entry.selectedExternalSubtitlePath);
        const auto existing =
            std::find_if(unique.begin(), unique.end(),
                         [&entry](const auto &item) { return item.mediaPath == entry.mediaPath; });
        if (existing == unique.end())
            unique.append(std::move(entry));
        else if (existing->lastUsedUtcMs <= entry.lastUsedUtcMs)
            *existing = std::move(entry);
    }
    std::stable_sort(unique.begin(), unique.end(), [](const auto &left, const auto &right) {
        return left.lastUsedUtcMs > right.lastUsedUtcMs;
    });
    if (unique.size() > TrackPreferenceStore::MaximumEntries)
        unique.erase(unique.begin() + TrackPreferenceStore::MaximumEntries, unique.end());
    return unique;
}

bool readEntry(QSettings &settings, int index, TrackPreferenceEntry &entry) {
    const auto prefix = QStringLiteral("entries/%1/").arg(index);
    bool sizeValid = false;
    bool modifiedValid = false;
    bool timestampValid = false;
    entry.mediaPath = settings.value(prefix + QStringLiteral("mediaPath")).toString();
    entry.fileSize = settings.value(prefix + QStringLiteral("fileSize")).toLongLong(&sizeValid);
    entry.modifiedUtcMs =
        settings.value(prefix + QStringLiteral("modifiedUtcMs")).toLongLong(&modifiedValid);
    entry.lastUsedUtcMs =
        settings.value(prefix + QStringLiteral("lastUsedUtcMs")).toLongLong(&timestampValid);
    if (!(sizeValid && modifiedValid && timestampValid))
        return false;
    const auto readId = [&settings, &prefix](const QString &name, std::optional<qint64> &target) {
        const auto key = prefix + name + QStringLiteral("/present");
        const auto present = settings.value(key).toBool();
        if (!present) {
            target.reset();
            return true;
        }
        bool valid = false;
        const auto id = settings.value(prefix + name + QStringLiteral("/id")).toLongLong(&valid);
        if (!valid || id < 0)
            return false;
        target = id;
        return true;
    };
    if (!readId(QStringLiteral("audioTrack"), entry.audioTrackId) ||
        !readId(QStringLiteral("subtitleTrack"), entry.subtitleTrackId))
        return false;
    entry.selectedExternalSubtitlePath =
        settings.value(prefix + QStringLiteral("selectedExternalSubtitlePath")).toString();
    entry.selectedExternalSubtitleStream =
        settings.value(prefix + QStringLiteral("selectedExternalSubtitleStream"), -1).toInt();
    bool countValid = false;
    const auto count =
        settings.value(prefix + QStringLiteral("externalSubtitleCount"), 0).toInt(&countValid);
    if (!countValid || count < 0 || count > 64)
        return false;
    for (int item = 0; item < count; ++item)
        entry.externalSubtitlePaths.append(
            settings.value(prefix + QStringLiteral("externalSubtitles/%1/path").arg(item))
                .toString());
    return validEntry(entry);
}

void writeId(QSettings &settings, const QString &prefix, const QString &name,
             const std::optional<qint64> &id) {
    settings.setValue(prefix + name + QStringLiteral("/present"), id.has_value());
    if (id)
        settings.setValue(prefix + name + QStringLiteral("/id"), *id);
}
} // namespace

TrackPreferenceStore::TrackPreferenceStore(QString filePath) : filePath_(std::move(filePath)) {}

QString TrackPreferenceStore::normalizedPath(const QString &path) {
    if (path.isEmpty() || path.size() > MaximumPathLength || path.contains(QChar::Null) ||
        !QFileInfo(path).isAbsolute())
        return {};
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile())
        return {};
    const auto canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QString{} : QDir::cleanPath(canonical);
}

bool TrackPreferenceStore::matchesFingerprint(const TrackPreferenceEntry &entry) {
    const auto path = normalizedPath(entry.mediaPath);
    if (path.isEmpty())
        return false;
    const QFileInfo info(path);
    return info.size() == entry.fileSize &&
           info.lastModified().toUTC().toMSecsSinceEpoch() == entry.modifiedUtcMs;
}

TrackPreferenceLoadResult TrackPreferenceStore::load() const {
    if (const auto error = validateExistingFile(filePath_); !error.isEmpty())
        return {{}, error};
    if (!QFileInfo::exists(filePath_))
        return {};
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return {{}, configurationError(filePath_)};
    if (settings.allKeys().isEmpty())
        return {};
    bool versionValid = false;
    const auto version = settings.value(QStringLiteral("schemaVersion")).toInt(&versionValid);
    if (!versionValid || version != SchemaVersion)
        return {{}, schemaError(filePath_)};
    bool countValid = false;
    const auto count = settings.value(QStringLiteral("entries/size")).toInt(&countValid);
    if (!countValid || count < 0 || count > 100000)
        return {{}, schemaError(filePath_)};
    QList<TrackPreferenceEntry> entries;
    bool invalid = false;
    for (int index = 0; index < count; ++index) {
        TrackPreferenceEntry entry;
        if (readEntry(settings, index, entry)) {
            entry.mediaPath = normalizedPath(entry.mediaPath);
            for (auto &path : entry.externalSubtitlePaths)
                path = normalizedSubtitlePath(path);
            if (!entry.selectedExternalSubtitlePath.isEmpty())
                entry.selectedExternalSubtitlePath =
                    normalizedSubtitlePath(entry.selectedExternalSubtitlePath);
            entries.append(std::move(entry));
        } else
            invalid = true;
    }
    return {normalizedEntries(entries), invalid ? schemaError(filePath_) : QString{}};
}

QString TrackPreferenceStore::save(const QList<TrackPreferenceEntry> &entries) const {
    if (const auto error = validateExistingFile(filePath_); !error.isEmpty())
        return error;
    const auto loaded = load();
    if (!loaded.error.isEmpty())
        return loaded.error;
    for (const auto &entry : entries) {
        if (!validEntry(entry) || !matchesFingerprint(entry))
            return schemaError(filePath_);
        for (const auto &path : entry.externalSubtitlePaths) {
            if (normalizedSubtitlePath(path).isEmpty())
                return schemaError(filePath_);
        }
        if (!entry.selectedExternalSubtitlePath.isEmpty() &&
            normalizedSubtitlePath(entry.selectedExternalSubtitlePath).isEmpty())
            return schemaError(filePath_);
    }
    const auto retained = normalizedEntries(entries);
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath()))
        return configurationError(filePath_);
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setAtomicSyncRequired(true);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    settings.clear();
    settings.setValue(QStringLiteral("schemaVersion"), SchemaVersion);
    settings.setValue(QStringLiteral("entries/size"), retained.size());
    for (qsizetype index = 0; index < retained.size(); ++index) {
        const auto prefix = QStringLiteral("entries/%1/").arg(index);
        const auto &entry = retained.at(index);
        settings.setValue(prefix + QStringLiteral("mediaPath"), entry.mediaPath);
        settings.setValue(prefix + QStringLiteral("fileSize"), entry.fileSize);
        settings.setValue(prefix + QStringLiteral("modifiedUtcMs"), entry.modifiedUtcMs);
        settings.setValue(prefix + QStringLiteral("lastUsedUtcMs"), entry.lastUsedUtcMs);
        writeId(settings, prefix, QStringLiteral("audioTrack"), entry.audioTrackId);
        writeId(settings, prefix, QStringLiteral("subtitleTrack"), entry.subtitleTrackId);
        settings.setValue(prefix + QStringLiteral("selectedExternalSubtitlePath"),
                          entry.selectedExternalSubtitlePath);
        settings.setValue(prefix + QStringLiteral("selectedExternalSubtitleStream"),
                          entry.selectedExternalSubtitleStream);
        settings.setValue(prefix + QStringLiteral("externalSubtitleCount"),
                          entry.externalSubtitlePaths.size());
        for (qsizetype item = 0; item < entry.externalSubtitlePaths.size(); ++item)
            settings.setValue(prefix + QStringLiteral("externalSubtitles/%1/path").arg(item),
                              entry.externalSubtitlePaths.at(item));
    }
    settings.sync();
    return settings.status() == QSettings::NoError ? QString{} : configurationError(filePath_);
}
