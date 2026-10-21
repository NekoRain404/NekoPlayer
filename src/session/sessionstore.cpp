#include "sessionstore.h"

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
QString configurationError(const QString &path) {
    return QCoreApplication::translate("SessionStore", "无法读写配置文件：%1").arg(path);
}

QString checkFile(const QString &path) {
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
    // QSettings caches parsed INI data by path, but FormatError belongs to an instance.
    // Validate a fresh snapshot so a second load/save cannot lose the parser error.
    QSettings settings(snapshot.fileName(), QSettings::IniFormat);
    const auto keys = settings.allKeys();
    if (settings.status() != QSettings::NoError)
        return configurationError(path);
    if (keys.isEmpty())
        return {};
    bool valid = false;
    const auto version = settings.value(QStringLiteral("schemaVersion")).toInt(&valid);
    if (!valid || version != 1)
        return QCoreApplication::translate("SessionStore", "配置版本不受支持，已保留原文件：%1")
            .arg(path);
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

SessionState normalized(SessionState state) {
    state.subtitles = state.subtitles.normalized();
    state.audio.volume =
        std::isfinite(state.audio.volume) ? std::clamp(state.audio.volume, 0.0, 100.0) : 100.0;
    QStringList paths;
    int current = -1;
    for (qsizetype row = 0; row < state.paths.size(); ++row) {
        const auto &path = state.paths.at(row);
        if (path.isEmpty() || path.contains(QChar::Null) || !QFileInfo(path).isAbsolute())
            continue;
        if (row == state.currentIndex)
            current = paths.size();
        paths.append(QDir::cleanPath(path));
    }
    state.paths = std::move(paths);
    state.currentIndex = current >= 0 ? current : (state.paths.isEmpty() ? -1 : 0);
    const QFileInfo directory(state.window.mediaDirectory);
    if (!directory.isAbsolute() || !directory.isDir())
        state.window.mediaDirectory.clear();
    return state;
}
} // namespace

SessionStore::SessionStore(QString filePath) : filePath_(std::move(filePath)) {}

SessionLoadResult SessionStore::load() const {
    if (const auto error = checkFile(filePath_); !error.isEmpty())
        return {{}, error};
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return {{}, configurationError(filePath_)};
    SessionState state;
    bool valid = false;
    const auto volume = settings.value(QStringLiteral("audio/volume"), 100).toDouble(&valid);
    state.audio.volume = valid ? volume : 100;
    state.audio.muted = readBool(settings, QStringLiteral("audio/muted"));
    state.hardwareDecodingEnabled =
        readBool(settings, QStringLiteral("video/hardwareDecoding"), true);
    state.subtitles.visible = readBool(settings, QStringLiteral("subtitles/visible"), true);
    const auto scale = settings.value(QStringLiteral("subtitles/scale"), 1).toDouble(&valid);
    state.subtitles.scale = valid ? scale : 1;
    const auto position =
        settings.value(QStringLiteral("subtitles/position"), 100).toDouble(&valid);
    state.subtitles.position = valid ? position : 100;
    state.subtitles.font =
        settings.value(QStringLiteral("subtitles/font"), QStringLiteral("sans-serif")).toString();
    state.paths = settings.value(QStringLiteral("playlist/paths")).toStringList();
    state.currentIndex = settings.value(QStringLiteral("playlist/currentIndex"), -1).toInt(&valid);
    if (!valid)
        state.currentIndex = -1;
    state.window.geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    state.window.playlistVisible = readBool(settings, QStringLiteral("window/playlistVisible"));
    state.window.mediaDirectory =
        settings.value(QStringLiteral("window/mediaDirectory")).toString();
    return {normalized(std::move(state)), {}};
}

QString SessionStore::save(const SessionState &state) const {
    if (const auto error = checkFile(filePath_); !error.isEmpty())
        return error;
    QSettings settings(filePath_, QSettings::IniFormat);
    settings.setAtomicSyncRequired(true);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    if (!QDir().mkpath(QFileInfo(filePath_).absolutePath()))
        return configurationError(filePath_);
    const auto saved = normalized(state);
    settings.setValue(QStringLiteral("schemaVersion"), 1);
    settings.setValue(QStringLiteral("audio/volume"), saved.audio.volume);
    settings.setValue(QStringLiteral("audio/muted"), saved.audio.muted);
    settings.setValue(QStringLiteral("video/hardwareDecoding"), saved.hardwareDecodingEnabled);
    settings.setValue(QStringLiteral("subtitles/visible"), saved.subtitles.visible);
    settings.setValue(QStringLiteral("subtitles/scale"), saved.subtitles.scale);
    settings.setValue(QStringLiteral("subtitles/position"), saved.subtitles.position);
    settings.setValue(QStringLiteral("subtitles/font"), saved.subtitles.font);
    settings.setValue(QStringLiteral("playlist/paths"), saved.paths);
    settings.setValue(QStringLiteral("playlist/currentIndex"), saved.currentIndex);
    settings.setValue(QStringLiteral("window/geometry"), saved.window.geometry);
    settings.setValue(QStringLiteral("window/playlistVisible"), saved.window.playlistVisible);
    settings.setValue(QStringLiteral("window/mediaDirectory"), saved.window.mediaDirectory);
    settings.sync();
    return settings.status() == QSettings::NoError ? QString{} : configurationError(filePath_);
}
