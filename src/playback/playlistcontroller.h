#pragma once

#include "shuffleorder.h"

#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>

class MpvController;
class PlaylistModel;

class PlaylistController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffleEnabled READ shuffleEnabled WRITE setShuffleEnabled NOTIFY
                   shuffleEnabledChanged)
  public:
    enum class RepeatMode { Off, One, All };
    Q_ENUM(RepeatMode)
    explicit PlaylistController(MpvController &player, QObject *parent = nullptr);

    [[nodiscard]] PlaylistModel *model();
    [[nodiscard]] const PlaylistModel *model() const;
    [[nodiscard]] bool hasNext() const;
    [[nodiscard]] bool hasPrevious() const;
    [[nodiscard]] RepeatMode repeatMode() const;
    [[nodiscard]] bool shuffleEnabled() const;
    [[nodiscard]] double pendingResumePosition() const;

  public slots:
    void addFiles(const QStringList &paths);
    void restoreFiles(const QStringList &paths, int currentIndex, double resumePosition = 0);
    void playAt(int row);
    void playOrPause();
    void playNext();
    void playPrevious();
    void removeAt(int row);
    void moveItem(int from, int to);
    void clear();
    void setRepeatMode(RepeatMode mode);
    void cycleRepeatMode();
    void setShuffleEnabled(bool enabled);

  signals:
    void currentChanged(int row);
    void queueFinished();
    void queueError(const QString &message);
    void repeatModeChanged(RepeatMode mode);
    void shuffleEnabledChanged(bool enabled);
    void navigationChanged();

  private slots:
    void playFinished();
    void mediaLoadFailed(const QString &message);

  private:
    enum class ScanResult { Started, Exhausted, Cancelled };
    ScanResult playFirstAvailable(int firstRow, int step = 1, bool wrap = false);
    ScanResult playShuffled(int step);
    void startRow(int row, int step);
    void updateFileLoop();
    void finishQueue();
    MpvController &player_;
    PlaylistModel *model_;
    bool exhausted_ = false;
    bool playbackQueued_ = false;
    quint64 playbackRevision_ = 0;
    int navigationStep_ = 1;
    RepeatMode repeatMode_ = RepeatMode::Off;
    bool shuffleEnabled_ = false;
    ShuffleOrder shuffle_;
    QSet<int> failedRows_;
    QPersistentModelIndex resumeEntry_;
    double resumePosition_ = 0;
};
