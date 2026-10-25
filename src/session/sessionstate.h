#pragma once

#include "playback/subtitlepreferences.h"
#include "playback/videocoloradjustment.h"
#include <QByteArray>
#include <QStringList>

inline constexpr int seekStepSecondsMinimum = 1;
inline constexpr int seekStepSecondsMaximum = 120;
inline constexpr int seekStepSecondsDefault = 5;

[[nodiscard]] constexpr int normalizedSeekStepSeconds(int seconds) {
    return seconds >= seekStepSecondsMinimum && seconds <= seekStepSecondsMaximum
               ? seconds
               : seekStepSecondsDefault;
}

struct AudioPreferences {
    double volume = 100;
    bool muted = false;
    QString outputDevice = QStringLiteral("auto");
};

struct TuningPreferences {
    double playbackSpeed = 1;
    double audioDelay = 0;
    double subtitleDelay = 0;

    [[nodiscard]] TuningPreferences normalized() const;
};

struct WindowPreferences {
    QByteArray geometry;
    bool playlistVisible = false;
    QString mediaDirectory;
    int playlistWidth = 0; // 0 = default width.
};

struct MediaLibraryPreferences {
    bool scanningEnabled = true;
    bool recursiveByDefault = false;
};

enum class MiddleClickAction { None, PlayPause, Mute, Fullscreen };

struct InteractionPreferences {
    bool singleClickPlayPause = true;
    bool doubleClickFullscreen = true;
    MiddleClickAction middleClick = MiddleClickAction::None;
    bool wheelAdjustsVolume = true;
    int wheelVolumeStep = 5; // 1..20
    bool wheelSeeksTimeline = true;
    bool playlistSingleClick = false;
    bool showRemainingTime = false;

    [[nodiscard]] InteractionPreferences normalized() const;
    bool operator==(const InteractionPreferences &) const = default;
};

struct SessionState {
    AudioPreferences audio;
    WindowPreferences window;
    QStringList paths;
    int currentIndex = -1;
    bool hardwareDecodingEnabled = true;
    SubtitlePreferences subtitles;
    double resumePosition = 0;
    TuningPreferences tuning;
    VideoColorAdjustment videoColor;
    MediaLibraryPreferences mediaLibrary;
    bool resumePlaybackPosition = true;
    int seekStepSeconds = seekStepSecondsDefault;
    InteractionPreferences interaction;
};

struct SessionLoadResult {
    SessionState state;
    QString error;
};
