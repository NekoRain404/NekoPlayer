#pragma once

#include <QObject>

class MpvController;
class PlaylistModel;

class PlaylistController final : public QObject {
    Q_OBJECT
  public:
    explicit PlaylistController(MpvController &player, QObject *parent = nullptr);

    [[nodiscard]] PlaylistModel *model();
    [[nodiscard]] const PlaylistModel *model() const;
    [[nodiscard]] bool hasNext() const;
    [[nodiscard]] bool hasPrevious() const;

  public slots:
    void addFiles(const QStringList &paths);
    void restoreFiles(const QStringList &paths, int currentIndex);
    void playAt(int row);
    void playOrPause();
    void playNext();
    void playPrevious();
    void clear();

  signals:
    void currentChanged(int row);
    void queueFinished();
    void queueError(const QString &message);

  private slots:
    void playFinished();
    void mediaLoadFailed(const QString &message);

  private:
    bool playFirstAvailable(int firstRow, int step = 1);
    void finishQueue();
    MpvController &player_;
    PlaylistModel *model_;
    bool exhausted_ = false;
    int navigationStep_ = 1;
};
