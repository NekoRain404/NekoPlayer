#pragma once

#include <QList>
#include <QString>

struct PlaybackHistoryEntry {
    QString path;
    double position = 0;
    double duration = 0;
    qint64 lastPlayedUtcMs = 0;
};

struct PlaybackHistoryLoadResult {
    QList<PlaybackHistoryEntry> entries;
    QString error;
};

class PlaybackHistoryStore final {
  public:
    static constexpr int MaximumEntries = 100;

    explicit PlaybackHistoryStore(QString filePath);

    [[nodiscard]] PlaybackHistoryLoadResult load() const;
    [[nodiscard]] QString save(const QList<PlaybackHistoryEntry> &entries) const;
    [[nodiscard]] static QString normalizedPath(const QString &path);

  private:
    QString filePath_;
};
