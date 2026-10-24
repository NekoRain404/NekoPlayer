#pragma once

#include "playback/subtitlepreferences.h"
#include "playback/videocoloradjustment.h"
#include <QByteArray>
#include <QStringList>

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
    int seekStepSeconds = 5;
    InteractionPreferences interaction;
};

struct SessionLoadResult {
    SessionState state;
    QString error;
};
