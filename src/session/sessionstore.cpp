#include "sessionstore.h"
#include "configfile.h"
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
#include <memory>
#include <utility>

namespace {
QString configurationError(const QString &path) {
    return QCoreApplication::translate("SessionStore", "无法读写配置文件：%1").arg(path);
}

QString openValidatedSettings(const QString &path, QTemporaryFile &snapshot,
                              std::unique_ptr<QSettings> &settings) {
    if (!QFileInfo::exists(path))
        return {};
    constexpr qint64 maximumConfigurationBytes = 4 * 1024 * 1024;
    const QFileInfo existing(path);
    if (!existing.isFile() || existing.size() > maximumConfigurationBytes)
        return configurationError(path);
    QFile source(path);
    if (!source.open(QIODevice::ReadOnly) || !snapshot.open())
        return configurationError(path);
    const auto bytes = source.readAll();
    if (source.error() != QFileDevice::NoError || snapshot.write(bytes) != bytes.size() ||
        !snapshot.flush())
        return configurationError(path);
    // Parse a snapshot: QSettings caches data by file name and a cached parse
    // error would otherwise hide a later format problem from this read.
    auto parsed = std::make_unique<QSettings>(snapshot.fileName(), QSettings::IniFormat);
    if (parsed->status() != QSettings::NoError)
        return configurationError(path);
    if (parsed->allKeys().isEmpty())
        return {};
    bool valid = false;
    const auto version = parsed->value(QStringLiteral("schemaVersion")).toInt(&valid);
    if (!valid || version != 1)
        return QCoreApplication::translate("SessionStore", "配置版本不受支持，已保留原文件：%1")
            .arg(path);
    settings = std::move(parsed);
    return {};
}

QString checkFile(const QString &path) {
    QTemporaryFile snapshot;
    std::unique_ptr<QSettings> settings;
    return openValidatedSettings(path, snapshot, settings);
}

bool readBool(QSettings &settings, const QString &key, bool fallback = false) {
    const auto value = settings.value(key).toString().toLower();
    if (value == QLatin1String("true") || value == QLatin1String("1"))
        return true;
    if (value == QLatin1String("false") || value == QLatin1String("0"))
        return false;
    return fallback;
}

MiddleClickAction readMiddleClickAction(QSettings &settings, const QString &key) {
    const auto value = settings.value(key).toString();
    if (value == QLatin1String("playPause"))
        return MiddleClickAction::PlayPause;
    if (value == QLatin1String("mute"))
        return MiddleClickAction::Mute;
    if (value == QLatin1String("fullscreen"))
        return MiddleClickAction::Fullscreen;
    return MiddleClickAction::None;
}

QString middleClickActionKey(MiddleClickAction action) {
    switch (action) {
    case MiddleClickAction::PlayPause:
        return QStringLiteral("playPause");
    case MiddleClickAction::Mute:
        return QStringLiteral("mute");
    case MiddleClickAction::Fullscreen:
        return QStringLiteral("fullscreen");
    case MiddleClickAction::None:
        break;
    }
    return QStringLiteral("none");
}

SessionState normalized(SessionState state) {
    state.subtitles = state.subtitles.normalized();
    state.tuning = state.tuning.normalized();
    state.videoColor = state.videoColor.normalized();
    state.interaction = state.interaction.normalized();
    state.languages = state.languages.normalized();
    state.seekStepSeconds = normalizedSeekStepSeconds(state.seekStepSeconds);
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
    if (state.window.playlistWidth != 0)
        state.window.playlistWidth = std::clamp(state.window.playlistWidth, 240, 1000);
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

InteractionPreferences InteractionPreferences::normalized() const {
    auto result = *this;
    result.wheelVolumeStep = std::clamp(result.wheelVolumeStep, 1, 20);
    return result;
}

LanguagePreferences LanguagePreferences::normalized() const {
    const auto normalizeLanguages = [](const QStringList &languages) {
        constexpr qsizetype maximumLanguages = 16;
        QStringList result;
        for (const auto &language : languages) {
            const auto trimmed = language.trimmed();
            if (trimmed.isEmpty())
                continue;
            if (result.size() >= maximumLanguages)
                break;
            result.append(trimmed);
        }
        return result;
    };
    return {normalizeLanguages(audioLanguages), normalizeLanguages(subtitleLanguages),
            autoLoadExternalSubtitles};
}

SessionStore::SessionStore(QString filePath) : filePath_(std::move(filePath)) {}

SessionLoadResult SessionStore::load() const {
    QTemporaryFile snapshot;
    std::unique_ptr<QSettings> parsed;
    if (const auto error = openValidatedSettings(filePath_, snapshot, parsed); !error.isEmpty())
        return {{}, error};
    if (!parsed)
        return {};
    auto &settings = *parsed;
    SessionState state;
    bool valid = false;
    const auto volume = settings.value(QStringLiteral("audio/volume"), 100).toDouble(&valid);
    state.audio.volume = valid ? volume : 100;
    state.audio.muted = readBool(settings, QStringLiteral("audio/muted"));
    state.audio.outputDevice =
        settings.value(QStringLiteral("audio/outputDevice"), QStringLiteral("auto")).toString();
    state.hardwareDecodingEnabled =
        readBool(settings, QStringLiteral("video/hardwareDecoding"), true);
    state.resumePlaybackPosition =
        readBool(settings, QStringLiteral("playback/resumePosition"), true);
    const auto seekStep =
        settings.value(QStringLiteral("playback/seekStepSeconds"), 5).toInt(&valid);
    state.seekStepSeconds = valid ? normalizedSeekStepSeconds(seekStep) : seekStepSecondsDefault;
    state.interaction.singleClickPlayPause =
        readBool(settings, QStringLiteral("interaction/singleClickPlayPause"), true);
    state.interaction.doubleClickFullscreen =
        readBool(settings, QStringLiteral("interaction/doubleClickFullscreen"), true);
    state.interaction.middleClick =
        readMiddleClickAction(settings, QStringLiteral("interaction/middleClick"));
    state.interaction.wheelAdjustsVolume =
        readBool(settings, QStringLiteral("interaction/wheelAdjustsVolume"), true);
    const auto wheelStep =
        settings.value(QStringLiteral("interaction/wheelVolumeStep"), 5).toInt(&valid);
    state.interaction.wheelVolumeStep = valid ? wheelStep : 5;
    state.interaction.wheelSeeksTimeline =
        readBool(settings, QStringLiteral("interaction/wheelSeeksTimeline"), true);
    state.interaction.playlistSingleClick =
        readBool(settings, QStringLiteral("interaction/playlistSingleClick"));
    state.interaction.showRemainingTime =
        readBool(settings, QStringLiteral("interaction/showRemainingTime"));
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
    state.subtitles.textColor =
        settings.value(QStringLiteral("subtitles/textColor"), QStringLiteral("#FFFFFFFF"))
            .toString();
    state.subtitles.outlineColor =
        settings.value(QStringLiteral("subtitles/outlineColor"), QStringLiteral("#FF000000"))
            .toString();
    state.subtitles.backgroundColor =
        settings.value(QStringLiteral("subtitles/backgroundColor"), QStringLiteral("#AF000000"))
            .toString();
    const auto outlineSize =
        settings.value(QStringLiteral("subtitles/outlineSize"), 1.65).toDouble(&valid);
    state.subtitles.outlineSize = valid ? outlineSize : 1.65;
    const auto shadowOffset =
        settings.value(QStringLiteral("subtitles/shadowOffset"), 0).toDouble(&valid);
    state.subtitles.shadowOffset = valid ? shadowOffset : 0;
    state.subtitles.codepage =
        settings.value(QStringLiteral("subtitles/codepage"), QStringLiteral("auto")).toString();
    state.languages.audioLanguages =
        settings.value(QStringLiteral("tracks/preferredAudioLanguages")).toStringList();
    state.languages.subtitleLanguages =
        settings.value(QStringLiteral("tracks/preferredSubtitleLanguages")).toStringList();
    state.languages.autoLoadExternalSubtitles =
        readBool(settings, QStringLiteral("subtitles/autoLoadExternal"));
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
    const auto playlistWidth =
        settings.value(QStringLiteral("window/playlistWidth"), 0).toInt(&valid);
    state.window.playlistWidth = valid ? playlistWidth : 0;
    state.window.alwaysOnTop = readBool(settings, QStringLiteral("window/alwaysOnTop"));
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
    settings.setValue(QStringLiteral("playback/resumePosition"), saved.resumePlaybackPosition);
    settings.setValue(QStringLiteral("playback/seekStepSeconds"), saved.seekStepSeconds);
    settings.setValue(QStringLiteral("interaction/singleClickPlayPause"),
                      saved.interaction.singleClickPlayPause);
    settings.setValue(QStringLiteral("interaction/doubleClickFullscreen"),
                      saved.interaction.doubleClickFullscreen);
    settings.setValue(QStringLiteral("interaction/middleClick"),
                      middleClickActionKey(saved.interaction.middleClick));
    settings.setValue(QStringLiteral("interaction/wheelAdjustsVolume"),
                      saved.interaction.wheelAdjustsVolume);
    settings.setValue(QStringLiteral("interaction/wheelVolumeStep"),
                      saved.interaction.wheelVolumeStep);
    settings.setValue(QStringLiteral("interaction/wheelSeeksTimeline"),
                      saved.interaction.wheelSeeksTimeline);
    settings.setValue(QStringLiteral("interaction/playlistSingleClick"),
                      saved.interaction.playlistSingleClick);
    settings.setValue(QStringLiteral("interaction/showRemainingTime"),
                      saved.interaction.showRemainingTime);
    settings.setValue(QStringLiteral("library/scanningEnabled"),
                      saved.mediaLibrary.scanningEnabled);
    settings.setValue(QStringLiteral("library/recursiveByDefault"),
                      saved.mediaLibrary.recursiveByDefault);
    settings.setValue(QStringLiteral("subtitles/visible"), saved.subtitles.visible);
    settings.setValue(QStringLiteral("subtitles/scale"), saved.subtitles.scale);
    settings.setValue(QStringLiteral("subtitles/position"), saved.subtitles.position);
    settings.setValue(QStringLiteral("subtitles/font"), saved.subtitles.font);
    settings.setValue(QStringLiteral("subtitles/textColor"), saved.subtitles.textColor);
    settings.setValue(QStringLiteral("subtitles/outlineColor"), saved.subtitles.outlineColor);
    settings.setValue(QStringLiteral("subtitles/backgroundColor"), saved.subtitles.backgroundColor);
    settings.setValue(QStringLiteral("subtitles/outlineSize"), saved.subtitles.outlineSize);
    settings.setValue(QStringLiteral("subtitles/shadowOffset"), saved.subtitles.shadowOffset);
    settings.setValue(QStringLiteral("subtitles/codepage"), saved.subtitles.codepage);
    settings.setValue(QStringLiteral("tracks/preferredAudioLanguages"),
                      saved.languages.audioLanguages);
    settings.setValue(QStringLiteral("tracks/preferredSubtitleLanguages"),
                      saved.languages.subtitleLanguages);
    settings.setValue(QStringLiteral("subtitles/autoLoadExternal"),
                      saved.languages.autoLoadExternalSubtitles);
    settings.setValue(QStringLiteral("playlist/paths"), saved.paths);
    settings.setValue(QStringLiteral("playlist/currentIndex"), saved.currentIndex);
    settings.setValue(QStringLiteral("playlist/resumePosition"), saved.resumePosition);
    settings.setValue(QStringLiteral("window/geometry"), saved.window.geometry);
    settings.setValue(QStringLiteral("window/playlistVisible"), saved.window.playlistVisible);
    settings.setValue(QStringLiteral("window/mediaDirectory"), saved.window.mediaDirectory);
    settings.setValue(QStringLiteral("window/playlistWidth"), saved.window.playlistWidth);
    settings.setValue(QStringLiteral("window/alwaysOnTop"), saved.window.alwaysOnTop);
    settings.setValue(QStringLiteral("playback/speed"), saved.tuning.playbackSpeed);
    settings.setValue(QStringLiteral("playback/audioDelay"), saved.tuning.audioDelay);
    settings.setValue(QStringLiteral("playback/subtitleDelay"), saved.tuning.subtitleDelay);
    settings.setValue(QStringLiteral("video/brightness"), saved.videoColor.brightness);
    settings.setValue(QStringLiteral("video/contrast"), saved.videoColor.contrast);
    settings.setValue(QStringLiteral("video/saturation"), saved.videoColor.saturation);
    settings.setValue(QStringLiteral("video/hue"), saved.videoColor.hue);
    settings.setValue(QStringLiteral("video/gamma"), saved.videoColor.gamma);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return configurationError(filePath_);
    restrictConfigurationFilePermissions(filePath_);
    return {};
}
