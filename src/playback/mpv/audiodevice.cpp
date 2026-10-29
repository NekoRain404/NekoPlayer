#include "playback/mpv/audiodevice.h"

#include <QCoreApplication>
#include <QSet>
#include <cstring>

namespace {
const mpv_node *field(const mpv_node &node, const char *name) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list)
        return nullptr;

    const auto &map = *node.u.list;
    if (map.num <= 0 || !map.keys || !map.values)
        return nullptr;
    for (int index = 0; index < map.num; ++index) {
        if (map.keys[index] && std::strcmp(map.keys[index], name) == 0)
            return &map.values[index];
    }
    return nullptr;
}

QString stringField(const mpv_node &node, const char *name) {
    const auto *value = field(node, name);
    if (!value || value->format != MPV_FORMAT_STRING || !value->u.string)
        return {};
    return QString::fromUtf8(value->u.string);
}
} // namespace

QList<AudioDevice> parseMpvAudioDevices(const mpv_node &node) {
    QList<AudioDevice> devices;
    QSet<QString> names;

    if (node.format == MPV_FORMAT_NODE_ARRAY && node.u.list && node.u.list->num > 0 &&
        node.u.list->values) {
        const auto &array = *node.u.list;
        devices.reserve(array.num + 1);
        for (int index = 0; index < array.num; ++index) {
            const auto &entry = array.values[index];
            auto name = stringField(entry, "name");
            if (name.trimmed().isEmpty() || names.contains(name))
                continue;

            names.insert(name);
            devices.append({std::move(name), stringField(entry, "description")});
        }
    }

    if (!names.contains(QStringLiteral("auto")))
        devices.prepend(
            {QStringLiteral("auto"), QCoreApplication::translate("AudioDevice", "自动选择")});
    return devices;
}
