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
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;

  signals:
    void playRequested(int row);

  private:
    void applyFilter();
    void connectSelectionModel();
    void refreshIcons();
    void selectSourceCurrent();
    void synchronizeCurrent();
    void handleStructureChange();
    void flushStructureRefresh();
    void refreshStructure();
    void updateItemCount();
    void updateEditingActions();
    void updateEmptyHint();
    [[nodiscard]] QList<int> selectedSourceRows() const;
    [[nodiscard]] int singleSelectedSourceRow() const;
    [[nodiscard]] int sourceRow(const QModelIndex &index) const;
    [[nodiscard]] QModelIndex sourceIndexForCurrent() const;
    PlaylistController &queue_;
    PlaylistModel *model_;
    QSortFilterProxyModel *filter_;
    QLineEdit *search_;
    QListView *view_;
    QLabel *emptyHint_;
    QAction *remove_;
    QAction *moveUp_;
    QAction *moveDown_;
    QAction *focusSearchAction_;
    QAction *clearSearchAction_;
    QPersistentModelIndex playingIndex_;
    QTimer *singleClickTimer_;
    QTimer *structureUpdateTimer_;
    QLabel *countLabel_ = nullptr;
    Qt::KeyboardModifiers lastClickModifiers_ = Qt::NoModifier;
    QPersistentModelIndex pendingClickIndex_;
    bool singleClickActivation_ = false;
    bool filtering_ = false;
    bool batchEditActive_ = false;
    bool structureRefreshPending_ = false;
};
