#pragma once

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QWidget>

class QAction;
class PlaylistController;
class QListView;
class QLabel;
class QEvent;
class QLineEdit;
class QSortFilterProxyModel;
class QTimer;
class PlaylistModel;

class PlaylistPane final : public QWidget {
    Q_OBJECT
  public:
    PlaylistPane(PlaylistController &queue, QAction *append, QAction *previous, QAction *next,
                 QAction *clear, QWidget *parent = nullptr);

    [[nodiscard]] QListView *view() const;
    [[nodiscard]] QList<QAction *> editingActions() const;
    [[nodiscard]] QList<QAction *> shortcutActions() const;
    void setSingleClickActivation(bool enabled);

  protected:
    void changeEvent(QEvent *event) override;

  signals:
    void playRequested(int row);

  private:
    void applyFilter();
    void connectSelectionModel();
    void refreshIcons();
    void selectSourceCurrent();
    void synchronizeCurrent();
    void updateItemCount();
    void updateEditingActions();
    [[nodiscard]] int sourceRow(const QModelIndex &index) const;
    [[nodiscard]] QModelIndex sourceIndexForCurrent() const;
    PlaylistController &queue_;
    PlaylistModel *model_;
    QSortFilterProxyModel *filter_;
    QLineEdit *search_;
    QListView *view_;
    QAction *remove_;
    QAction *moveUp_;
    QAction *moveDown_;
    QAction *focusSearchAction_;
    QAction *clearSearchAction_;
    QPersistentModelIndex playingIndex_;
    QTimer *singleClickTimer_;
    QPersistentModelIndex pendingClickIndex_;
    bool singleClickActivation_ = false;
    bool filtering_ = false;
};
