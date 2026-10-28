#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct M3u8Entry {
    QString source;
    double durationSeconds = -1; // -1 means unknown, matching #EXTINF:-1
    QString title;
    bool operator==(const M3u8Entry &) const = default;
};

struct M3u8LoadResult {
    QStringList paths;
    int currentIndex = -1;
    QString error;
    // Full entries with optional #EXTINF metadata. `paths` mirrors source for
    // callers that only need the queue.
    QList<M3u8Entry> entries;
};

class M3u8Store final {
  public:
    [[nodiscard]] static M3u8LoadResult load(const QString &filePath);
    [[nodiscard]] static QString save(const QString &filePath, const QStringList &paths,
                                      int currentIndex);
    [[nodiscard]] static QString saveEntries(const QString &filePath,
                                             const QList<M3u8Entry> &entries, int currentIndex);
};
