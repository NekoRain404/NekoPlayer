#pragma once

#include <QList>
#include <QString>

struct MediaInformation;
struct MediaTrack;
struct PlaybackState;

struct SeekProgressPresentation {
    bool available = false;
    int value = 0;
    QString text;
};

SeekProgressPresentation formatSeekProgress(const PlaybackState &state);
QString formatPlaybackOverlayInformation(const MediaInformation &info, const PlaybackState &state,
                                         const QList<MediaTrack> &tracks = {});
