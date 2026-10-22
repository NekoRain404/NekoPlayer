#pragma once

#include "playback/trackselectionpreference.h"
#include <QList>
#include <QString>

struct TrackPreferenceEntry {
    QString mediaPath;
    qint64 fileSize = -1;
    qint64 modifiedUtcMs = -1;
    qint64 lastUsedUtcMs = 0;
    TrackSelectionPreference selection;
};

struct TrackPreferenceLoadResult {
    QList<TrackPreferenceEntry> entries;
    QString error;
};

class TrackPreferenceStore final {
  public:
    static constexpr int MaximumEntries = 100;
    static constexpr qsizetype MaximumPathLength = 8192;

    explicit TrackPreferenceStore(QString filePath);

    [[nodiscard]] TrackPreferenceLoadResult load() const;
    [[nodiscard]] QString save(const QList<TrackPreferenceEntry> &entries) const;
    [[nodiscard]] static QString normalizedPath(const QString &path);
    [[nodiscard]] static bool matchesFingerprint(const TrackPreferenceEntry &entry);

  private:
    QString filePath_;
};
