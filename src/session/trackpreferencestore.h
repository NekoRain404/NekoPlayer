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

enum class TrackPreferenceState { Invalid, Missing, Current, Replaced };

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
    // Missing media keeps its entry: the file may return (removable or network
    // drive). Only a file that still exists with a different fingerprint is
    // treated as replaced.
    [[nodiscard]] static TrackPreferenceState state(const TrackPreferenceEntry &entry);
    [[nodiscard]] static bool matchesFingerprint(const TrackPreferenceEntry &entry);

  private:
    QString filePath_;
};
