#pragma once

#include "playlist/m3u8store.h"

// Dispatches playlist load/save by suffix: M3U/M3U8, PLS and XSPF. The M3U8
// path keeps its current-row extension; PLS/XSPF preserve order and per-entry
// title/duration only.
class PlaylistFile final {
  public:
    [[nodiscard]] static M3u8LoadResult load(const QString &filePath);
    [[nodiscard]] static QString save(const QString &filePath, const QList<M3u8Entry> &entries,
                                      int currentIndex);
};
