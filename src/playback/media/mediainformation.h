#pragma once

#include <QString>

struct MediaInformation {
    QString fileFormat;
    qint64 fileSize = -1;
    QString videoCodec;
    qint64 videoWidth = 0;
    qint64 videoHeight = 0;
    QString videoPixelFormat;
    QString videoPrimaries;
    QString videoTransfer;
    double containerFrameRate = 0;
    QString audioCodec;
    qint64 audioSampleRate = 0;
    QString audioChannels;
    QString audioSampleFormat;

    [[nodiscard]] bool isHdr() const {
        return videoTransfer.compare(QLatin1String("pq"), Qt::CaseInsensitive) == 0 ||
               videoTransfer.compare(QLatin1String("hlg"), Qt::CaseInsensitive) == 0;
    }

    bool operator==(const MediaInformation &) const = default;
};
