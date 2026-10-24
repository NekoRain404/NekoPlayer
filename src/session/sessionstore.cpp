#include "sessionstore.h"
#include "playback/mediasource.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryFile>
#include <QUrl>
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
    state.tuning = state.tuning.normalized();
    state.videoColor = state.videoColor.normalized();
    state.audio.volume =
        std::isfinite(state.audio.volume) ? std::clamp(state.audio.volume, 0.0, 100.0) : 100.0;
    if (state.audio.outputDevice.isEmpty() || state.audio.outputDevice.contains(QChar::Null))
        state.audio.outputDevice = QStringLiteral("auto");
    QStringList paths;
    qsizetype current = -1;
    for (qsizetype row = 0; row < state.paths.size(); ++row) {
        const auto &path = state.paths.at(row);
        // Persisted local paths must remain independent of the working directory.
        if (!QDir::isAbsolutePath(path) && QUrl(path, QUrl::StrictMode).scheme().isEmpty())
            continue;
        const auto source = MediaSource::normalize(path);
        if (source.isEmpty())
            continue;
        if (row == state.currentIndex)
            current = paths.size();
        paths.append(source);
    }
    state.paths = std::move(paths);
    if (current < 0 || !std::isfinite(state.resumePosition) || state.resumePosition < 0 ||
        MediaSource::isNetwork(state.paths.at(current)))
        state.resumePosition = 0;
    state.currentIndex =
        current >= 0 ? static_cast<int>(current) : (state.paths.isEmpty() ? -1 : 0);
    const QFileInfo directory(state.window.mediaDirectory);
    if (!directory.isAbsolute() || !directory.isDir())
        state.window.mediaDirectory.clear();
    return state;
}
} // namespace

TuningPreferences TuningPreferences::normalized() const {
    return {
        std::isfinite(playbackSpeed) ? std::clamp(playbackSpeed, 0.25, 4.0) : 1.0,
        std::isfinite(audioDelay) ? std::clamp(audioDelay, -10.0, 10.0) : 0.0,
        std::isfinite(subtitleDelay) ? std::clamp(subtitleDelay, -600.0, 600.0) : 0.0,
    };
}

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
    state.audio.outputDevice =
        settings.value(QStringLiteral("audio/outputDevice"), QStringLiteral("auto")).toString();
    state.hardwareDecodingEnabled =
        readBool(settings, QStringLiteral("video/hardwareDecoding"), true);
    state.mediaLibrary.scanningEnabled =
        readBool(settings, QStringLiteral("library/scanningEnabled"), true);
    state.mediaLibrary.recursiveByDefault =
        readBool(settings, QStringLiteral("library/recursiveByDefault"));
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
    const auto resume =
        settings.value(QStringLiteral("playlist/resumePosition"), 0).toDouble(&valid);
    state.resumePosition = valid ? resume : 0;
    state.window.geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    state.window.playlistVisible = readBool(settings, QStringLiteral("window/playlistVisible"));
    state.window.mediaDirectory =
        settings.value(QStringLiteral("window/mediaDirectory")).toString();
    const auto speed = settings.value(QStringLiteral("playback/speed"), 1).toDouble(&valid);
    state.tuning.playbackSpeed = valid ? speed : 1;
    const auto audioDelay =
        settings.value(QStringLiteral("playback/audioDelay"), 0).toDouble(&valid);
    state.tuning.audioDelay = valid ? audioDelay : 0;
    const auto subtitleDelay =
        settings.value(QStringLiteral("playback/subtitleDelay"), 0).toDouble(&valid);
    state.tuning.subtitleDelay = valid ? subtitleDelay : 0;
    const auto colorValue = [&settings](const QString &key) {
        bool valid = false;
        const double value = settings.value(key, 0).toDouble(&valid);
        return valid ? value : 0.0;
    };
    state.videoColor = {colorValue(QStringLiteral("video/brightness")),
                        colorValue(QStringLiteral("video/contrast")),
                        colorValue(QStringLiteral("video/saturation")),
                        colorValue(QStringLiteral("video/hue")),
                        colorValue(QStringLiteral("video/gamma"))};
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
    settings.setValue(QStringLiteral("audio/outputDevice"), saved.audio.outputDevice);
    settings.setValue(QStringLiteral("video/hardwareDecoding"), saved.hardwareDecodingEnabled);
    settings.setValue(QStringLiteral("library/scanningEnabled"),
                      saved.mediaLibrary.scanningEnabled);
    settings.setValue(QStringLiteral("library/recursiveByDefault"),
                      saved.mediaLibrary.recursiveByDefault);
    settings.setValue(QStringLiteral("subtitles/visible"), saved.subtitles.visible);
    settings.setValue(QStringLiteral("subtitles/scale"), saved.subtitles.scale);
    settings.setValue(QStringLiteral("subtitles/position"), saved.subtitles.position);
    settings.setValue(QStringLiteral("subtitles/font"), saved.subtitles.font);
    settings.setValue(QStringLiteral("playlist/paths"), saved.paths);
    settings.setValue(QStringLiteral("playlist/currentIndex"), saved.currentIndex);
    settings.setValue(QStringLiteral("playlist/resumePosition"), saved.resumePosition);
    settings.setValue(QStringLiteral("window/geometry"), saved.window.geometry);
    settings.setValue(QStringLiteral("window/playlistVisible"), saved.window.playlistVisible);
    settings.setValue(QStringLiteral("window/mediaDirectory"), saved.window.mediaDirectory);
    settings.setValue(QStringLiteral("playback/speed"), saved.tuning.playbackSpeed);
    settings.setValue(QStringLiteral("playback/audioDelay"), saved.tuning.audioDelay);
    settings.setValue(QStringLiteral("playback/subtitleDelay"), saved.tuning.subtitleDelay);
    settings.setValue(QStringLiteral("video/brightness"), saved.videoColor.brightness);
    settings.setValue(QStringLiteral("video/contrast"), saved.videoColor.contrast);
    settings.setValue(QStringLiteral("video/saturation"), saved.videoColor.saturation);
    settings.setValue(QStringLiteral("video/hue"), saved.videoColor.hue);
    settings.setValue(QStringLiteral("video/gamma"), saved.videoColor.gamma);
    settings.sync();
    return settings.status() == QSettings::NoError ? QString{} : configurationError(filePath_);
}
