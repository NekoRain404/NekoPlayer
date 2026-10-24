#pragma once

#include <QPersistentModelIndex>
#include <QWidget>

class QAction;
class PlaylistController;
class QListView;
class QLabel;
class QEvent;
class QTimer;

class PlaylistPane final : public QWidget {
    Q_OBJECT
  public:
    PlaylistPane(PlaylistController &queue, QAction *append, QAction *previous, QAction *next,
                 QAction *clear, QWidget *parent = nullptr);

    [[nodiscard]] QListView *view() const;
    [[nodiscard]] QList<QAction *> editingActions() const;
    void setSingleClickActivation(bool enabled);

  protected:
    void changeEvent(QEvent *event) override;

  signals:
    void playRequested(int row);

  private:
    void refreshIcons();
    void synchronizeCurrent();
    void updateItemCount();
    void updateEditingActions();
    PlaylistController &queue_;
    QListView *view_;
    QAction *remove_;
    QAction *moveUp_;
    QAction *moveDown_;
    QPersistentModelIndex playingIndex_;
    QTimer *singleClickTimer_;
    QPersistentModelIndex pendingClickIndex_;
    bool singleClickActivation_ = false;
};
