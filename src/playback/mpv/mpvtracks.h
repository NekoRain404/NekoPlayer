#pragma once

#include "playback/media/mediatrack.h"
#include <QList>

struct mpv_node;

// Copies borrowed mpv event data into an owned snapshot; never frees the input.
QList<MediaTrack> parseMpvTracks(const mpv_node &node);

// Internal IDs are stable on reload; external IDs must match by file and stream order.
qint64 trackIdForRestore(const QList<MediaTrack> &saved, const QList<MediaTrack> &current,
                         MediaTrack::Type type);
bool trackSelectionRestored(const QList<MediaTrack> &saved, const QList<MediaTrack> &current);
