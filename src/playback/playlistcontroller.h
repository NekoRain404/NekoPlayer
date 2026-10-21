#pragma once

#include <QObject>
#include <QSet>

class MpvController;
class PlaylistModel;

class PlaylistController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RepeatMode repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
  public:
    enum class RepeatMode { Off, One, All };
    Q_ENUM(RepeatMode)
    explicit PlaylistController(MpvController &player, QObject *parent = nullptr);

    [[nodiscard]] PlaylistModel *model();
    [[nodiscard]] const PlaylistModel *model() const;
    [[nodiscard]] bool hasNext() const;
    [[nodiscard]] bool hasPrevious() const;
    [[nodiscard]] RepeatMode repeatMode() const;

  public slots:
    void addFiles(const QStringList &paths);
    void restoreFiles(const QStringList &paths, int currentIndex);
    void playAt(int row);
    void playOrPause();
    void playNext();
    void playPrevious();
    void removeAt(int row);
    void moveItem(int from, int to);
    void clear();
    void setRepeatMode(RepeatMode mode);
    void cycleRepeatMode();

  signals:
    void currentChanged(int row);
    void queueFinished();
    void queueError(const QString &message);
    void repeatModeChanged(RepeatMode mode);

  private slots:
    void playFinished();
    void mediaLoadFailed(const QString &message);

  private:
    enum class ScanResult { Started, Exhausted, Cancelled };
    ScanResult playFirstAvailable(int firstRow, int step = 1, bool wrap = false);
    void updateFileLoop();
    void finishQueue();
    MpvController &player_;
    PlaylistModel *model_;
    bool exhausted_ = false;
    bool playbackQueued_ = false;
    quint64 playbackRevision_ = 0;
    int navigationStep_ = 1;
    RepeatMode repeatMode_ = RepeatMode::Off;
    QSet<int> failedRows_;
};
