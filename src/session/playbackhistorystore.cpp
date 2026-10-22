#include "playbackhistorystore.h"

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
} // namespace

PlaybackHistoryStore::PlaybackHistoryStore(QString filePath) : filePath_(std::move(filePath)) {}

QString PlaybackHistoryStore::normalizedPath(const QString &path) {
    if (path.isEmpty() || path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
        return {};
    return QDir::cleanPath(path);
}

PlaybackHistoryLoadResult PlaybackHistoryStore::load() const {
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

    bool schemaValid = false;
    const auto version = settings.value(QStringLiteral("schemaVersion")).toInt(&schemaValid);
    if (!schemaValid || version != SchemaVersion)
        return {{}, schemaError(filePath_)};
    bool countValid = false;
    const auto count = settings.value(QStringLiteral("entries/size")).toInt(&countValid);
    if (!countValid || count < 0 || count > 100000)
        return {{}, schemaError(filePath_)};

    QList<PlaybackHistoryEntry> entries;
    bool invalidEntryFound = false;
    for (int index = 0; index < count; ++index) {
        PlaybackHistoryEntry entry;
        if (readEntry(settings, index, entry)) {
            entry.path = normalizedPath(entry.path);
            entries.append(std::move(entry));
        } else
            invalidEntryFound = true;
    }
    return {uniqueEntries(entries), invalidEntryFound ? schemaError(filePath_) : QString{}};
}

QString PlaybackHistoryStore::save(const QList<PlaybackHistoryEntry> &entries) const {
    if (const auto error = validateExistingFile(filePath_); !error.isEmpty())
        return error;
    if (const auto loaded = load(); !loaded.error.isEmpty())
        return loaded.error;

    QList<PlaybackHistoryEntry> normalized;
    normalized.reserve(entries.size());
    for (const auto &entry : entries) {
        if (!validEntry(entry))
            return schemaError(filePath_);
        normalized.append(entry);
    }
    const auto retained = uniqueEntries(normalized);

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
    return settings.status() == QSettings::NoError ? QString{} : configurationError(filePath_);
}
