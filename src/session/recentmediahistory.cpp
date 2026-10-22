#include "recentmediahistory.h"

#include "playback/mediasource.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <utility>

namespace {
constexpr int SchemaVersion = 1;
constexpr qsizetype MaximumEntries = 20;
constexpr qsizetype MaximumSourceLength = 16384;
// Leaves room for twenty sources even when JSON escapes every UTF-16 code unit.
constexpr qint64 MaximumFileBytes = 4 * 1024 * 1024;

QString fileError(const QString &path, const QString &detail) {
    return QCoreApplication::translate("RecentMediaHistory", "无法读写最近播放记录：%1（%2）")
        .arg(path, detail);
}

QString formatError(const QString &path) {
    return QCoreApplication::translate("RecentMediaHistory",
                                       "最近播放记录格式无效或版本不受支持，已保留原文件：%1")
        .arg(path);
}

QString normalizedSource(const QString &source) {
    if (source.size() > MaximumSourceLength)
        return {};
    const auto normalized = MediaSource::normalize(source);
    return normalized.size() <= MaximumSourceLength ? normalized : QString{};
}

QString readEntries(const QString &path, QStringList &entries) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink())
        return {};
    if (!info.isFile())
        return fileError(path, QCoreApplication::translate("RecentMediaHistory", "不是可读文件"));
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return fileError(path, file.errorString());
    if (file.size() > MaximumFileBytes)
        return formatError(path);
    const auto bytes = file.read(MaximumFileBytes + 1);
    if (file.error() != QFileDevice::NoError)
        return fileError(path, file.errorString());
    if (bytes.size() > MaximumFileBytes)
        return formatError(path);

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return formatError(path);
    const auto object = document.object();
    const auto version = object.value(QStringLiteral("schemaVersion"));
    const auto sources = object.value(QStringLiteral("entries"));
    if (object.size() != 2 || !version.isDouble() || version.toDouble() != SchemaVersion ||
        !sources.isArray())
        return formatError(path);
    const auto array = sources.toArray();
    if (array.size() > MaximumEntries)
        return formatError(path);
    QStringList parsed;
    for (const auto &value : array) {
        if (!value.isString())
            return formatError(path);
        const auto normalized = normalizedSource(value.toString());
        if (normalized.isEmpty())
            return formatError(path);
        if (!parsed.contains(normalized))
            parsed.append(normalized);
    }
    entries = std::move(parsed);
    return {};
}
} // namespace

RecentMediaHistory::RecentMediaHistory(QObject *parent) : QObject(parent) {}

const QStringList &RecentMediaHistory::entries() const { return entries_; }

QString RecentMediaHistory::load(const QString &path) {
    path_ = path;
    QStringList loaded;
    loadError_ = path.isEmpty() ? QString{} : readEntries(path, loaded);
    if (!loadError_.isEmpty())
        return loadError_;
    if (entries_ != loaded) {
        entries_ = std::move(loaded);
        emit changed();
    }
    return {};
}

QString RecentMediaHistory::save() const {
    if (path_.isEmpty())
        return {};
    if (!loadError_.isEmpty())
        return loadError_;
    QStringList existing;
    if (const auto error = readEntries(path_, existing); !error.isEmpty())
        return error;
    if (!QDir().mkpath(QFileInfo(path_).absolutePath()))
        return fileError(path_, tr("无法创建父目录"));

    QJsonArray sources;
    for (const auto &source : entries_)
        sources.append(source);
    const auto bytes = QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), SchemaVersion},
                                                 {QStringLiteral("entries"), sources}})
                           .toJson();
    QSaveFile file(path_);
    if (!file.open(QIODevice::WriteOnly))
        return fileError(path_, file.errorString());
    if (file.write(bytes) != bytes.size())
        return fileError(path_, file.errorString());
    return file.commit() ? QString{} : fileError(path_, file.errorString());
}

void RecentMediaHistory::remember(const QString &source) {
    const auto normalized = normalizedSource(source);
    if (normalized.isEmpty() || (!entries_.isEmpty() && entries_.front() == normalized))
        return;
    entries_.removeAll(normalized);
    entries_.prepend(normalized);
    if (entries_.size() > MaximumEntries)
        entries_.removeLast();
    emit changed();
}

void RecentMediaHistory::clear() {
    if (entries_.isEmpty())
        return;
    entries_.clear();
    emit changed();
}
