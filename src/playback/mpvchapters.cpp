#include "mpvchapters.h"

#include <cmath>
#include <cstring>

QList<MediaChapter> parseMpvChapters(const mpv_node &node) {
    QList<MediaChapter> chapters;
    if (node.format != MPV_FORMAT_NODE_ARRAY || !node.u.list)
        return chapters;
    chapters.reserve(node.u.list->num);
    for (int index = 0; index < node.u.list->num; ++index) {
        const auto &entry = node.u.list->values[index];
        if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list)
            continue;
        const auto &map = *entry.u.list;
        MediaChapter chapter{index, {}, -1};
        for (int field = 0; field < map.num; ++field) {
            const auto &value = map.values[field];
            if (std::strcmp(map.keys[field], "title") == 0 && value.format == MPV_FORMAT_STRING)
                chapter.title = QString::fromUtf8(value.u.string);
            if (std::strcmp(map.keys[field], "time") == 0 && value.format == MPV_FORMAT_DOUBLE)
                chapter.start = value.u.double_;
        }
        if (std::isfinite(chapter.start) && chapter.start >= 0)
            chapters.append(std::move(chapter));
    }
    return chapters;
}
