#pragma once

#include <QList>
#include <QString>
#include <mpv/client.h>

struct AudioDevice {
    QString name;
    QString description;
    friend bool operator==(const AudioDevice &, const AudioDevice &) = default;
};

QList<AudioDevice> parseMpvAudioDevices(const mpv_node &node);
