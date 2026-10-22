#pragma once

#include <QString>

struct MediaInformation;
struct PlaybackState;

struct SeekProgressPresentation {
    bool available = false;
    int value = 0;
    QString text;
};

SeekProgressPresentation formatSeekProgress(const PlaybackState &state);
QString formatPlaybackOverlayInformation(const MediaInformation &info, const PlaybackState &state);
