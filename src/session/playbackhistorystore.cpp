#include "playbackhistorystore.h"
#include "configfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryFile>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr int SchemaVersion = 1;

QString configurationError(const QString &path) {
    return QCoreApplication::translate("PlaybackHistoryStore", "无法读写配置文件：%1").arg(path);
}

QString schemaError(const QString &path) {
    return QCoreApplication::translate("PlaybackHistoryStore",
                                       "播放历史格式不受支持，已保留原文件：%1")
        .arg(path);
}

bool validEntry(const PlaybackHistoryEntry &entry) {
    return !PlaybackHistoryStore::normalizedPath(entry.path).isEmpty() &&
           std::isfinite(entry.position) && entry.position >= 0 && std::isfinite(entry.duration) &&
           entry.duration > 0 && entry.lastPlayedUtcMs > 0;
}

QList<PlaybackHistoryEntry> newestEntries(QList<PlaybackHistoryEntry> entries) {
    std::stable_sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        return left.lastPlayedUtcMs > right.lastPlayedUtcMs;
    });
    if (entries.size() > PlaybackHistoryStore::MaximumEntries)
        entries.erase(entries.begin() + PlaybackHistoryStore::MaximumEntries, entries.end());
    return entries;
}

QList<PlaybackHistoryEntry> uniqueEntries(const QList<PlaybackHistoryEntry> &entries) {
    QList<PlaybackHistoryEntry> unique;
    for (const auto &entry : entries) {
        const auto path = PlaybackHistoryStore::normalizedPath(entry.path);
        auto found = std::find_if(unique.begin(), unique.end(), [&path](const auto &candidate) {
            return candidate.path == path;
        });
        auto normalized = entry;
        normalized.path = path;
        if (found == unique.end())
            unique.append(std::move(normalized));
        else
            *found = std::move(normalized);
    }
    return newestEntries(std::move(unique));
}

bool readEntry(QSettings &settings, int index, PlaybackHistoryEntry &entry) {
    const auto prefix = QStringLiteral("entries/%1/").arg(index);
    bool positionValid = false;
    bool durationValid = false;
    bool timestampValid = false;
    entry.path = settings.value(prefix + QStringLiteral("path")).toString();
    entry.position = settings.value(prefix + QStringLiteral("position")).toDouble(&positionValid);
    entry.duration = settings.value(prefix + QStringLiteral("duration")).toDouble(&durationValid);
    entry.lastPlayedUtcMs =
        settings.value(prefix + QStringLiteral("lastPlayedUtcMs")).toLongLong(&timestampValid);
    return positionValid && durationValid && timestampValid && validEntry(entry);
}

struct StoreContents {
    bool exists = false;
    bool wellFormed = false;
    QList<PlaybackHistoryEntry> entries;
    QString error;
};

StoreContents readContents(const QString &path) {
    StoreContents contents;
    if (!QFileInfo::exists(path))
        return contents;
    contents.exists = true;
    constexpr qint64 maximumConfigurationBytes = 4 * 1024 * 1024;
    const QFileInfo existing(path);
    if (!existing.isFile() || existing.size() > maximumConfigurationBytes) {
        contents.error = configurationError(path);
        return contents;
    }
    QFile source(path);
    QTemporaryFile snapshot;
    if (!source.open(QIODevice::ReadOnly) || !snapshot.open()) {
        contents.error = configurationError(path);
        return contents;
    }
    const auto bytes = source.readAll();
    if (source.error() != QFileDevice::NoError || snapshot.write(bytes) != bytes.size() ||
        !snapshot.flush()) {
        contents.error = configurationError(path);
        return contents;
    }
    // Parse a snapshot: QSettings caches data by file name and a cached parse
    // error would otherwise hide a later format problem from this read.
    QSettings settings(snapshot.fileName(), QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        contents.error = configurationError(path);
        return contents;
    }
    if (settings.allKeys().isEmpty())
        return contents;
    bool versionValid = false;
    const auto version = settings.value(QStringLiteral("schemaVersion")).toInt(&versionValid);
    if (!versionValid || version != SchemaVersion) {
        contents.error = schemaError(path);
        return contents;
    }
    bool countValid = false;
    const auto count = settings.value(QStringLiteral("entries/size")).toInt(&countValid);
    if (!countValid || count < 0 || count > 100000) {
        contents.error = schemaError(path);
        return contents;
    }
    QList<PlaybackHistoryEntry> parsed;
    bool dropped = false;
    for (int index = 0; index < count; ++index) {
        PlaybackHistoryEntry entry;
        if (!readEntry(settings, index, entry)) {
            // A single invalid entry is quarantined, not a fatal configuration
            // error: the next save rewrites only the entries that parsed.
            dropped = true;
            continue;
        }
        entry.path = PlaybackHistoryStore::normalizedPath(entry.path);
        parsed.append(std::move(entry));
    }
    contents.entries = uniqueEntries(parsed);
    contents.wellFormed = !dropped && contents.entries.size() == count;
    return contents;
}

bool sameEntries(const QList<PlaybackHistoryEntry> &left,
                 const QList<PlaybackHistoryEntry> &right) {
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        const auto &a = left.at(index);
        const auto &b = right.at(index);
        if (a.path != b.path || a.position != b.position || a.duration != b.duration ||
            a.lastPlayedUtcMs != b.lastPlayedUtcMs)
            return false;
    }
    return true;
}
} // namespace

PlaybackHistoryStore::PlaybackHistoryStore(QString filePath) : filePath_(std::move(filePath)) {}

QString PlaybackHistoryStore::normalizedPath(const QString &path) {
    if (path.isEmpty() || path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
        return {};
    return QDir::cleanPath(path);
}

PlaybackHistoryLoadResult PlaybackHistoryStore::load() const {
    const auto contents = readContents(filePath_);
    return {contents.entries, contents.error};
}

QString PlaybackHistoryStore::save(const QList<PlaybackHistoryEntry> &entries) const {
    // One parse supplies both validation and the on-disk payload used for the
    // no-op check; loading a second time cannot reveal anything new.
    const auto contents = readContents(filePath_);
    if (!contents.error.isEmpty())
        return contents.error;

    QList<PlaybackHistoryEntry> normalized;
    normalized.reserve(entries.size());
    for (const auto &entry : entries) {
        if (!validEntry(entry))
            return schemaError(filePath_);
        normalized.append(entry);
    }
    const auto retained = uniqueEntries(normalized);
    if (contents.exists && contents.wellFormed && sameEntries(contents.entries, retained)) {
        restrictConfigurationFilePermissions(filePath_);
        return {};
    }

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
        settings.setValue(prefix + QStringLiteral("path"), entry.path);
        settings.setValue(prefix + QStringLiteral("position"), entry.position);
        settings.setValue(prefix + QStringLiteral("duration"), entry.duration);
        settings.setValue(prefix + QStringLiteral("lastPlayedUtcMs"), entry.lastPlayedUtcMs);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    restrictConfigurationFilePermissions(filePath_);
    return {};
}
