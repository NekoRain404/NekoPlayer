#pragma once

#include <QWidget>

class QAction;
class PlaylistModel;
class QListView;

class PlaylistPane final : public QWidget {
    Q_OBJECT
  public:
    PlaylistPane(PlaylistModel *model, QAction *append, QAction *previous, QAction *next,
                 QAction *clear, QWidget *parent = nullptr);

    [[nodiscard]] QListView *view() const;

  signals:
    void playRequested(int row);

  private:
    QListView *view_;
};
