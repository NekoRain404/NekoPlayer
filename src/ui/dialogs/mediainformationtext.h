#pragma once

#include <QString>

struct MediaInformation;
struct PlaybackState;

QString formatMediaInformation(const MediaInformation &information, const PlaybackState &state);
