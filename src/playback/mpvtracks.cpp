#include "mpvtracks.h"

#include <QHash>
#include <cstring>
#include <mpv/client.h>

namespace {
const mpv_node *field(const mpv_node &node, const char *name) {
    if (node.format != MPV_FORMAT_NODE_MAP || !node.u.list)
        return nullptr;
    const auto &map = *node.u.list;
    for (int index = 0; index < map.num; ++index) {
        if (std::strcmp(map.keys[index], name) == 0)
            return &map.values[index];
    }
    return nullptr;
}

QString text(const mpv_node &node, const char *name) {
    const auto *value = field(node, name);
    return value && value->format == MPV_FORMAT_STRING ? QString::fromUtf8(value->u.string)
                                                       : QString{};
}

bool flag(const mpv_node &node, const char *name) {
    const auto *value = field(node, name);
    return value && value->format == MPV_FORMAT_FLAG && value->u.flag;
}
} // namespace

QList<MediaTrack> parseMpvTracks(const mpv_node &node) {
    QList<MediaTrack> tracks;
    if (node.format != MPV_FORMAT_NODE_ARRAY || !node.u.list)
        return tracks;
    tracks.reserve(node.u.list->num);
    for (int index = 0; index < node.u.list->num; ++index) {
        const auto &entry = node.u.list->values[index];
        const auto type = text(entry, "type");
        if (type != QLatin1String("audio") && type != QLatin1String("sub"))
            continue;
        const auto *id = field(entry, "id");
        if (!id || id->format != MPV_FORMAT_INT64 || id->u.int64 <= 0)
            continue;
        tracks.append(
            {id->u.int64,
             type == QLatin1String("audio") ? MediaTrack::Type::Audio : MediaTrack::Type::Subtitle,
             text(entry, "lang"), text(entry, "title"), text(entry, "codec"),
             flag(entry, "external"), flag(entry, "selected"), text(entry, "external-filename")});
    }
    return tracks;
}

qint64 trackIdForRestore(const QList<MediaTrack> &saved, const QList<MediaTrack> &current,
                         MediaTrack::Type type) {
    QHash<QString, int> ordinals;
    for (const auto &track : saved) {
        if (track.type != type)
            continue;
        const int ordinal = track.external ? ordinals[track.externalPath]++ : 0;
        if (!track.selected)
            continue;
        int candidateOrdinal = 0;
        for (const auto &candidate : current) {
            if (candidate.type != type || candidate.external != track.external)
                continue;
            if (track.external ? (candidate.externalPath == track.externalPath &&
                                  candidateOrdinal++ == ordinal)
                               : candidate.id == track.id) {
                return candidate.id;
            }
        }
        break;
    }
    return 0;
}

bool trackSelectionRestored(const QList<MediaTrack> &saved, const QList<MediaTrack> &current) {
    for (const auto type : {MediaTrack::Type::Audio, MediaTrack::Type::Subtitle}) {
        qint64 selected = 0;
        for (const auto &track : current) {
            if (track.type == type && track.selected)
                selected = track.id;
        }
        if (selected != trackIdForRestore(saved, current, type))
            return false;
    }
    return true;
}
