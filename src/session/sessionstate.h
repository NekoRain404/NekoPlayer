#pragma once

#include <QByteArray>
#include <QStringList>

struct AudioPreferences {
    double volume = 100;
    bool muted = false;
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
};

struct SessionLoadResult {
    SessionState state;
    QString error;
};
