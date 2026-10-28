#include "trackpreferencestore.h"
#include "configfile.h"

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

QString absoluteCleanPath(const QString &path) {
    if (path.isEmpty() || path.size() > TrackPreferenceStore::MaximumPathLength ||
        path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
        return {};
    return QDir::cleanPath(path);
}

QString normalizedExternalPath(const QString &path) {
    if (path.isEmpty() || path.size() > TrackPreferenceStore::MaximumPathLength ||
        path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
        return {};
    const QFileInfo info(path);
    const auto canonical = info.exists() ? info.canonicalFilePath() : QString{};
    return QDir::cleanPath(canonical.isEmpty() ? path : canonical);
}

bool validOptionalId(const std::optional<qint64> &id) { return !id || *id >= 0; }

bool validEntry(const TrackPreferenceEntry &entry) {
    if (normalizedExternalPath(entry.mediaPath).isEmpty() || entry.fileSize < 0 ||
        entry.modifiedUtcMs <= 0 || entry.lastUsedUtcMs <= 0 ||
        !validOptionalId(entry.selection.audioTrackId) ||
        !validOptionalId(entry.selection.subtitleTrackId) ||
        entry.selection.selectedExternalAudioStream < -1 ||
        entry.selection.selectedExternalAudioStream > 65535 ||
        entry.selection.externalAudioPaths.size() > 64 ||
        entry.selection.selectedExternalSubtitleStream < -1 ||
        entry.selection.externalSubtitlePaths.size() > 64)
        return false;
    if (!entry.selection.selectedExternalAudioPath.isEmpty() &&
        normalizedExternalPath(entry.selection.selectedExternalAudioPath).isEmpty())
        return false;
    for (const auto &path : entry.selection.externalAudioPaths) {
        if (normalizedExternalPath(path).isEmpty())
            return false;
    }
    if (!entry.selection.selectedExternalSubtitlePath.isEmpty() &&
        normalizedExternalPath(entry.selection.selectedExternalSubtitlePath).isEmpty())
        return false;
    for (const auto &path : entry.selection.externalSubtitlePaths) {
        if (normalizedExternalPath(path).isEmpty())
            return false;
    }
    return true;
}

void normalizeExternalPaths(TrackSelectionPreference &selection) {
    for (auto &path : selection.externalAudioPaths)
        path = normalizedExternalPath(path);
    if (!selection.selectedExternalAudioPath.isEmpty())
        selection.selectedExternalAudioPath =
            normalizedExternalPath(selection.selectedExternalAudioPath);
    for (auto &path : selection.externalSubtitlePaths)
        path = normalizedExternalPath(path);
    if (!selection.selectedExternalSubtitlePath.isEmpty())
        selection.selectedExternalSubtitlePath =
            normalizedExternalPath(selection.selectedExternalSubtitlePath);
}

QList<TrackPreferenceEntry> normalizedEntries(const QList<TrackPreferenceEntry> &entries) {
    QList<TrackPreferenceEntry> unique;
    unique.reserve(entries.size());
    for (auto entry : entries) {
        const auto state = TrackPreferenceStore::state(entry);
        if (state == TrackPreferenceState::Invalid || state == TrackPreferenceState::Replaced)
            continue;
        entry.mediaPath = TrackPreferenceStore::normalizedPath(entry.mediaPath);
        normalizeExternalPaths(entry.selection);
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
        const auto presence = settings.value(key).toString().toLower();
        if (presence != QStringLiteral("true") && presence != QStringLiteral("false"))
            return false;
        const auto present = presence == QStringLiteral("true");
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
    if (!readId(QStringLiteral("audioTrack"), entry.selection.audioTrackId) ||
        !readId(QStringLiteral("subtitleTrack"), entry.selection.subtitleTrackId))
        return false;
    // Audio fields are optional additions to schema 1; older files have internal IDs only.
    entry.selection.selectedExternalAudioPath =
        settings.value(prefix + QStringLiteral("selectedExternalAudioPath")).toString();
    bool audioStreamValid = false;
    entry.selection.selectedExternalAudioStream =
        settings.value(prefix + QStringLiteral("selectedExternalAudioStream"), -1)
            .toInt(&audioStreamValid);
    bool audioCountValid = false;
    const auto audioCount =
        settings.value(prefix + QStringLiteral("externalAudioCount"), 0).toInt(&audioCountValid);
    if (!audioStreamValid || !audioCountValid || audioCount < 0 || audioCount > 64)
        return false;
    for (int item = 0; item < audioCount; ++item)
        entry.selection.externalAudioPaths.append(
            settings.value(prefix + QStringLiteral("externalAudio/%1/path").arg(item)).toString());
    entry.selection.selectedExternalSubtitlePath =
        settings.value(prefix + QStringLiteral("selectedExternalSubtitlePath")).toString();
    bool streamValid = false;
    entry.selection.selectedExternalSubtitleStream =
        settings.value(prefix + QStringLiteral("selectedExternalSubtitleStream"), -1)
            .toInt(&streamValid);
    if (!streamValid)
        return false;
    bool countValid = false;
    const auto count =
        settings.value(prefix + QStringLiteral("externalSubtitleCount"), 0).toInt(&countValid);
    if (!countValid || count < 0 || count > 64)
        return false;
    for (int item = 0; item < count; ++item)
        entry.selection.externalSubtitlePaths.append(
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

struct StoreContents {
    bool exists = false;
    bool wellFormed = false;
    QList<TrackPreferenceEntry> entries;
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
    QList<TrackPreferenceEntry> parsed;
    for (int index = 0; index < count; ++index) {
        TrackPreferenceEntry entry;
        if (!readEntry(settings, index, entry)) {
            contents.error = schemaError(path);
            return contents;
        }
        // Replaced media is stale data; temporarily missing media is not,
        // because the file may come back with the same fingerprint.
        const auto state = TrackPreferenceStore::state(entry);
        if (state == TrackPreferenceState::Invalid || state == TrackPreferenceState::Replaced)
            continue;
        entry.mediaPath = TrackPreferenceStore::normalizedPath(entry.mediaPath);
        normalizeExternalPaths(entry.selection);
        parsed.append(std::move(entry));
    }
    contents.entries = normalizedEntries(parsed);
    // A drop during normalization means disk does not hold the normalized
    // payload, so the next save must be allowed to rewrite it.
    contents.wellFormed = contents.entries.size() == count;
    return contents;
}

bool sameSelection(const TrackSelectionPreference &left, const TrackSelectionPreference &right) {
    return left.audioTrackId == right.audioTrackId &&
           left.subtitleTrackId == right.subtitleTrackId &&
           left.selectedExternalAudioPath == right.selectedExternalAudioPath &&
           left.selectedExternalAudioStream == right.selectedExternalAudioStream &&
           left.externalAudioPaths == right.externalAudioPaths &&
           left.selectedExternalSubtitlePath == right.selectedExternalSubtitlePath &&
           left.selectedExternalSubtitleStream == right.selectedExternalSubtitleStream &&
           left.externalSubtitlePaths == right.externalSubtitlePaths;
}

bool sameEntries(const QList<TrackPreferenceEntry> &left,
                 const QList<TrackPreferenceEntry> &right) {
    if (left.size() != right.size())
        return false;
    for (qsizetype index = 0; index < left.size(); ++index) {
        const auto &a = left.at(index);
        const auto &b = right.at(index);
        if (a.mediaPath != b.mediaPath || a.fileSize != b.fileSize ||
            a.modifiedUtcMs != b.modifiedUtcMs || a.lastUsedUtcMs != b.lastUsedUtcMs ||
            !sameSelection(a.selection, b.selection))
            return false;
    }
    return true;
}
} // namespace

TrackPreferenceStore::TrackPreferenceStore(QString filePath) : filePath_(std::move(filePath)) {}

QString TrackPreferenceStore::normalizedPath(const QString &path) {
    const auto clean = absoluteCleanPath(path);
    if (clean.isEmpty())
        return {};
    const QFileInfo info(clean);
    if (!info.exists() || !info.isFile())
        return clean;
    const auto canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? clean : QDir::cleanPath(canonical);
}

TrackPreferenceState TrackPreferenceStore::state(const TrackPreferenceEntry &entry) {
    const auto path = absoluteCleanPath(entry.mediaPath);
    if (path.isEmpty())
        return TrackPreferenceState::Invalid;
    const QFileInfo info(path);
    if (!info.exists())
        return TrackPreferenceState::Missing;
    if (!info.isFile())
        return TrackPreferenceState::Replaced;
    if (info.size() != entry.fileSize ||
        info.lastModified().toUTC().toMSecsSinceEpoch() != entry.modifiedUtcMs)
        return TrackPreferenceState::Replaced;
    return TrackPreferenceState::Current;
}

bool TrackPreferenceStore::matchesFingerprint(const TrackPreferenceEntry &entry) {
    return state(entry) == TrackPreferenceState::Current;
}

TrackPreferenceLoadResult TrackPreferenceStore::load() const {
    const auto contents = readContents(filePath_);
    return {contents.entries, contents.error};
}

QString TrackPreferenceStore::save(const QList<TrackPreferenceEntry> &entries) const {
    // One parse supplies both validation and the on-disk payload used for the
    // no-op check; loading a second time cannot reveal anything new.
    const auto contents = readContents(filePath_);
    if (!contents.error.isEmpty())
        return contents.error;
    for (const auto &entry : entries) {
        if (!validEntry(entry))
            return schemaError(filePath_);
    }
    const auto retained = normalizedEntries(entries);
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
        settings.setValue(prefix + QStringLiteral("mediaPath"), entry.mediaPath);
        settings.setValue(prefix + QStringLiteral("fileSize"), entry.fileSize);
        settings.setValue(prefix + QStringLiteral("modifiedUtcMs"), entry.modifiedUtcMs);
        settings.setValue(prefix + QStringLiteral("lastUsedUtcMs"), entry.lastUsedUtcMs);
        writeId(settings, prefix, QStringLiteral("audioTrack"), entry.selection.audioTrackId);
        writeId(settings, prefix, QStringLiteral("subtitleTrack"), entry.selection.subtitleTrackId);
        settings.setValue(prefix + QStringLiteral("selectedExternalAudioPath"),
                          entry.selection.selectedExternalAudioPath);
        settings.setValue(prefix + QStringLiteral("selectedExternalAudioStream"),
                          entry.selection.selectedExternalAudioStream);
        settings.setValue(prefix + QStringLiteral("externalAudioCount"),
                          entry.selection.externalAudioPaths.size());
        for (qsizetype item = 0; item < entry.selection.externalAudioPaths.size(); ++item)
            settings.setValue(prefix + QStringLiteral("externalAudio/%1/path").arg(item),
                              entry.selection.externalAudioPaths.at(item));
        settings.setValue(prefix + QStringLiteral("selectedExternalSubtitlePath"),
                          entry.selection.selectedExternalSubtitlePath);
        settings.setValue(prefix + QStringLiteral("selectedExternalSubtitleStream"),
                          entry.selection.selectedExternalSubtitleStream);
        settings.setValue(prefix + QStringLiteral("externalSubtitleCount"),
                          entry.selection.externalSubtitlePaths.size());
        for (qsizetype item = 0; item < entry.selection.externalSubtitlePaths.size(); ++item)
            settings.setValue(prefix + QStringLiteral("externalSubtitles/%1/path").arg(item),
                              entry.selection.externalSubtitlePaths.at(item));
    }
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    restrictConfigurationFilePermissions(filePath_);
    return {};
}
