#pragma once

#include <QString>

struct MediaInformation {
    QString fileFormat;
    qint64 fileSize = -1;
    QString videoCodec;
    qint64 videoWidth = 0;
    qint64 videoHeight = 0;
    QString videoPixelFormat;
    double containerFrameRate = 0;
    QString audioCodec;
    qint64 audioSampleRate = 0;
    QString audioChannels;
    QString audioSampleFormat;

    bool operator==(const MediaInformation &) const = default;
};
