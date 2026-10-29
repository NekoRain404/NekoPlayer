#pragma once

#include <QString>

struct MediaTrack {
    enum class Type { Audio, Subtitle };
    qint64 id = 0;
    Type type = Type::Audio;
    QString language;
    QString title;
    QString codec;
    bool external = false;
    bool selected = false;
    QString externalPath;

    bool operator==(const MediaTrack &) const = default;
};
