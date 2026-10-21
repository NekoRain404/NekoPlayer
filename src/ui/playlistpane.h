#pragma once

#include <QPersistentModelIndex>
#include <QWidget>

class QAction;
class PlaylistController;
class QListView;

class PlaylistPane final : public QWidget {
    Q_OBJECT
  public:
    PlaylistPane(PlaylistController &queue, QAction *append, QAction *previous, QAction *next,
                 QAction *clear, QWidget *parent = nullptr);

    [[nodiscard]] QListView *view() const;
    [[nodiscard]] QList<QAction *> editingActions() const;

  signals:
    void playRequested(int row);

  private:
    void synchronizeCurrent();
    void updateEditingActions();
    PlaylistController &queue_;
    QListView *view_;
    QAction *remove_;
    QAction *moveUp_;
    QAction *moveDown_;
    QPersistentModelIndex playingIndex_;
};
