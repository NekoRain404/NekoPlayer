#pragma once

#include "playback/subtitlepreferences.h"
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

struct SessionState {
    AudioPreferences audio;
    WindowPreferences window;
    QStringList paths;
    int currentIndex = -1;
    bool hardwareDecodingEnabled = true;
    SubtitlePreferences subtitles;
    double resumePosition = 0;
    TuningPreferences tuning;
};

struct SessionLoadResult {
    SessionState state;
    QString error;
};
