#pragma once

#include "playlistmodel.h"
#include "shuffleorder.h"

#include <QObject>
#include <QPersistentModelIndex>
#include <QSet>
#include <functional>

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
    [[nodiscard]] bool stopAfterCurrent() const;
    [[nodiscard]] double pendingResumePosition() const;
    [[nodiscard]] QList<int> missingFileRows() const;
    void setResumePositionResolver(std::function<double(const QString &)> resolver);

  public slots:
    void addFiles(const QStringList &paths);
    void restoreFiles(const QStringList &paths, int currentIndex, double resumePosition = 0);
    void playAt(int row);
    void playOrPause();
    void playNext();
    void playPrevious();
    void removeAt(int row);
    void removeRowsAt(const QList<int> &rows);
    int removeMissingFiles();
    void sortPlaylist(PlaylistModel::SortKey key, Qt::SortOrder order);
    void moveItem(int from, int to);
    void clear();
    void setRepeatMode(RepeatMode mode);
    void cycleRepeatMode();
    void setShuffleEnabled(bool enabled);
    void setStopAfterCurrent(bool enabled);

  signals:
    void currentChanged(int row);
    void batchEditStarted();
    void batchEditFinished();
    void queueFinished();
    void queueError(const QString &message);
    void repeatModeChanged(RepeatMode mode);
    void shuffleEnabledChanged(bool enabled);
    void stopAfterCurrentChanged(bool enabled);
    void navigationChanged();
    void playbackPositionSnapshot(const QString &path, double position, double duration);
    void mediaPlaybackFinished(const QString &path);

  private slots:
    void playFinished();
    void mediaLoadFailed(const QString &message);

  private:
    enum class ScanResult { Started, Exhausted, Cancelled };
    ScanResult playFirstAvailable(int firstRow, int step = 1, bool wrap = false);
    ScanResult playShuffled(int step);
    void startRow(int row, int step);
    void snapshotCurrentPlayback();
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
    bool stopAfterCurrent_ = false;
    ShuffleOrder shuffle_;
    int editBatchDepth_ = 0;
    bool shufflePrunePending_ = false;
    QSet<int> failedRows_;
    QPersistentModelIndex resumeEntry_;
    double resumePosition_ = 0;
    std::function<double(const QString &)> resumePositionResolver_;
};
