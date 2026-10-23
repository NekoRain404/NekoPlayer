#include "playlistpane.h"

#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
class CurrentItemDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

  protected:
    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        if (index.data(PlaylistModel::CurrentRole).toBool())
            option->font.setBold(true);
    }
};

QToolButton *actionButton(QAction *action, QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    button->setFocusPolicy(Qt::StrongFocus);
    const auto updateDescription = [button, action] {
        button->setAccessibleName(action->text());
        button->setToolTip(action->text());
    };
    updateDescription();
    QObject::connect(action, &QAction::changed, button, updateDescription);
    return button;
}
} // namespace

PlaylistPane::PlaylistPane(PlaylistController &queue, QAction *append, QAction *previous,
                           QAction *next, QAction *clear, QWidget *parent)
    : QWidget(parent), queue_(queue), view_(new QListView(this)),
      remove_(new QAction(tr("从列表移除"), this)), moveUp_(new QAction(tr("上移所选项"), this)),
      moveDown_(new QAction(tr("下移所选项"), this)) {
    auto *model = queue_.model();
    setObjectName(QStringLiteral("playlistPane"));
    setMinimumWidth(240);
    view_->setObjectName(QStringLiteral("playlistView"));
    view_->setModel(model);
    view_->setTextElideMode(Qt::ElideMiddle);
    view_->setItemDelegate(new CurrentItemDelegate(view_));
    view_->setSelectionMode(QAbstractItemView::SingleSelection);
    view_->setDragDropMode(QAbstractItemView::InternalMove);
    view_->setDefaultDropAction(Qt::MoveAction);
    view_->setDragDropOverwriteMode(false);
    view_->setDropIndicatorShown(true);
    remove_->setObjectName(QStringLiteral("removePlaylistItemAction"));
    moveUp_->setObjectName(QStringLiteral("movePlaylistItemUpAction"));
    moveDown_->setObjectName(QStringLiteral("movePlaylistItemDownAction"));
    remove_->setShortcut(QKeySequence(Qt::Key_Delete));
    moveUp_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
    moveDown_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
    remove_->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
    moveUp_->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    moveDown_->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    view_->setAccessibleName(tr("播放列表"));
    view_->setToolTip(tr("双击播放；可拖动排序"));
    for (auto *action : editingActions()) {
        action->setShortcutContext(Qt::WidgetShortcut);
        view_->addAction(action);
    }
    view_->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(remove_, &QAction::triggered, this,
            [this] { queue_.removeAt(view_->currentIndex().row()); });
    connect(moveUp_, &QAction::triggered, this, [this] {
        const auto row = view_->currentIndex().row();
        queue_.moveItem(row, row - 1);
    });
    connect(moveDown_, &QAction::triggered, this, [this] {
        const auto row = view_->currentIndex().row();
        queue_.moveItem(row, row + 1);
    });
    connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &PlaylistPane::updateEditingActions);
    connect(model, &PlaylistModel::currentIndexChanged, this, &PlaylistPane::synchronizeCurrent);
    connect(model, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateEditingActions);
    connect(model, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateEditingActions);
    connect(model, &PlaylistModel::rowsMoved, this, &PlaylistPane::updateEditingActions);
    connect(model, &PlaylistModel::modelReset, this, &PlaylistPane::updateEditingActions);
    connect(model, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateItemCount);
    connect(model, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateItemCount);
    connect(model, &PlaylistModel::modelReset, this, &PlaylistPane::updateItemCount);
    view_->setAlternatingRowColors(false);
    view_->setUniformItemSizes(true);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("playlistHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 2, 4, 2);
    headerLayout->setSpacing(6);
    auto *title = new QLabel(header);
    title->setObjectName(QStringLiteral("playlistTitle"));
    title->setText(tr("播放列表"));
    auto *count = new QLabel(header);
    count->setObjectName(QStringLiteral("playlistCount"));
    count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    headerLayout->addWidget(count);
    layout->addWidget(header);
    layout->addWidget(view_, 1);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(actionButton(previous, this));
    buttons->addWidget(actionButton(next, this));
    buttons->addStretch();
    buttons->addWidget(actionButton(append, this));
    buttons->addWidget(actionButton(remove_, this));
    buttons->addWidget(actionButton(clear, this));
    layout->addLayout(buttons);
    connect(view_, &QListView::activated, this,
            [this](const QModelIndex &index) { emit playRequested(index.row()); });
    synchronizeCurrent();
    updateItemCount();
    updateEditingActions();
}

QListView *PlaylistPane::view() const { return view_; }

QList<QAction *> PlaylistPane::editingActions() const { return {remove_, moveUp_, moveDown_}; }

void PlaylistPane::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() != QEvent::LanguageChange)
        return;
    remove_->setText(tr("从列表移除"));
    moveUp_->setText(tr("上移所选项"));
    moveDown_->setText(tr("下移所选项"));
    if (auto *title = findChild<QLabel *>(QStringLiteral("playlistTitle")))
        title->setText(tr("播放列表"));
    view_->setAccessibleName(tr("播放列表"));
    view_->setToolTip(tr("双击播放；可拖动排序"));
    updateItemCount();
}

void PlaylistPane::updateItemCount() {
    if (auto *count = findChild<QLabel *>(QStringLiteral("playlistCount")))
        count->setText(tr("%1 项").arg(queue_.model()->rowCount()));
}

void PlaylistPane::synchronizeCurrent() {
    const auto current = queue_.model()->index(queue_.model()->currentIndex());
    // Reordering changes row numbers, but not the playing entry's identity.
    // Preserve a separate editing selection until playback changes entries.
    if (playingIndex_ != current) {
        playingIndex_ = current;
        view_->setCurrentIndex(current);
    }
    updateEditingActions();
}

void PlaylistPane::updateEditingActions() {
    const auto row = view_->currentIndex().row();
    remove_->setEnabled(row >= 0);
    moveUp_->setEnabled(row > 0);
    moveDown_->setEnabled(row >= 0 && row + 1 < queue_.model()->rowCount());
}
