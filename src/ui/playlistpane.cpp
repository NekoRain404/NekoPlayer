#include "playlistpane.h"
#include "appearanceicons.h"
#include "playlistitemdelegate.h"

#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
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
    : QWidget(parent), queue_(queue), model_(queue_.model()),
      filter_(new QSortFilterProxyModel(this)), search_(new QLineEdit(this)),
      view_(new QListView(this)), remove_(new QAction(tr("从列表移除"), this)),
      moveUp_(new QAction(tr("上移所选项"), this)), moveDown_(new QAction(tr("下移所选项"), this)),
      singleClickTimer_(new QTimer(this)) {
    setObjectName(QStringLiteral("playlistPane"));
    setMinimumWidth(240);
    filter_->setSourceModel(model_);
    filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filter_->setFilterKeyColumn(0);
    filter_->setDynamicSortFilter(true);
    search_->setObjectName(QStringLiteral("playlistSearchEdit"));
    search_->setPlaceholderText(tr("搜索播放列表"));
    search_->setAccessibleName(tr("搜索播放列表"));
    search_->setClearButtonEnabled(true);
    view_->setObjectName(QStringLiteral("playlistView"));
    view_->setModel(model_);
    view_->setTextElideMode(Qt::ElideMiddle);
    view_->setItemDelegate(new PlaylistItemDelegate(view_));
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
    refreshIcons();
    view_->setAccessibleName(tr("播放列表"));
    view_->setToolTip(tr("双击播放；可拖动排序"));
    for (auto *action : editingActions()) {
        action->setShortcutContext(Qt::WidgetShortcut);
        view_->addAction(action);
    }
    view_->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(remove_, &QAction::triggered, this,
            [this] { queue_.removeAt(sourceRow(view_->currentIndex())); });
    connect(moveUp_, &QAction::triggered, this, [this] {
        const auto row = sourceRow(view_->currentIndex());
        queue_.moveItem(row, row - 1);
    });
    connect(moveDown_, &QAction::triggered, this, [this] {
        const auto row = sourceRow(view_->currentIndex());
        queue_.moveItem(row, row + 1);
    });
    connectSelectionModel();
    connect(search_, &QLineEdit::textChanged, this, &PlaylistPane::applyFilter);
    auto *clearSearchShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), search_);
    clearSearchShortcut->setContext(Qt::WidgetShortcut);
    connect(clearSearchShortcut, &QShortcut::activated, search_, &QLineEdit::clear);
    auto *focusSearchShortcut = new QShortcut(QKeySequence::Find, this);
    focusSearchShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this] {
        search_->setFocus(Qt::ShortcutFocusReason);
        search_->selectAll();
    });
    connect(model_, &PlaylistModel::currentIndexChanged, this, &PlaylistPane::synchronizeCurrent);
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::rowsMoved, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateItemCount);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateItemCount);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistPane::updateItemCount);
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
    layout->addWidget(search_);
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
    singleClickTimer_->setSingleShot(true);
    singleClickTimer_->setTimerType(Qt::PreciseTimer);
    connect(singleClickTimer_, &QTimer::timeout, this, [this] {
        const auto pending = pendingClickIndex_;
        pendingClickIndex_ = QPersistentModelIndex{};
        if (pending.isValid())
            emit playRequested(pending.row());
    });
    connect(view_, &QListView::clicked, this, [this](const QModelIndex &index) {
        if (!singleClickActivation_)
            return;
        const auto row = sourceRow(index);
        if (row < 0) {
            pendingClickIndex_ = QPersistentModelIndex{};
            return;
        }
        pendingClickIndex_ = model_->index(row);
        singleClickTimer_->start(QApplication::doubleClickInterval());
    });
    connect(view_, &QListView::doubleClicked, this, [this] {
        singleClickTimer_->stop();
        pendingClickIndex_ = QPersistentModelIndex{};
    });
    connect(view_, &QListView::activated, this, [this](const QModelIndex &index) {
        singleClickTimer_->stop();
        pendingClickIndex_ = QPersistentModelIndex{};
        const auto row = sourceRow(index);
        if (row >= 0)
            emit playRequested(row);
    });
    synchronizeCurrent();
    updateItemCount();
    updateEditingActions();
}

QListView *PlaylistPane::view() const { return view_; }

QList<QAction *> PlaylistPane::editingActions() const { return {remove_, moveUp_, moveDown_}; }

void PlaylistPane::setSingleClickActivation(bool enabled) {
    if (singleClickActivation_ == enabled)
        return;
    singleClickActivation_ = enabled;
    if (!enabled) {
        singleClickTimer_->stop();
        pendingClickIndex_ = QPersistentModelIndex{};
    }
}

void PlaylistPane::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        refreshIcons();
    if (event->type() != QEvent::LanguageChange)
        return;
    remove_->setText(tr("从列表移除"));
    moveUp_->setText(tr("上移所选项"));
    moveDown_->setText(tr("下移所选项"));
    if (auto *title = findChild<QLabel *>(QStringLiteral("playlistTitle")))
        title->setText(tr("播放列表"));
    search_->setPlaceholderText(tr("搜索播放列表"));
    search_->setAccessibleName(tr("搜索播放列表"));
    view_->setAccessibleName(tr("播放列表"));
    view_->setToolTip(tr("双击播放；可拖动排序"));
    updateItemCount();
}

void PlaylistPane::refreshIcons() {
    remove_->setIcon(AppearanceIcons::icon(AppearanceIcons::Symbol::Remove, palette()));
    moveUp_->setIcon(AppearanceIcons::icon(AppearanceIcons::Symbol::MoveUp, palette()));
    moveDown_->setIcon(AppearanceIcons::icon(AppearanceIcons::Symbol::MoveDown, palette()));
}

void PlaylistPane::applyFilter() {
    const auto text = search_->text().trimmed();
    const bool filtering = !text.isEmpty();
    filter_->setFilterFixedString(text);
    filtering_ = filtering;
    if (filtering) {
        if (view_->model() != filter_) {
            view_->setModel(filter_);
            view_->setDragDropMode(QAbstractItemView::NoDragDrop);
            connectSelectionModel();
        }
    } else if (view_->model() != model_) {
        view_->setModel(model_);
        view_->setDragDropMode(QAbstractItemView::InternalMove);
        connectSelectionModel();
    }
    selectSourceCurrent();
    updateItemCount();
    updateEditingActions();
}

void PlaylistPane::connectSelectionModel() {
    if (view_->selectionModel())
        connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
                &PlaylistPane::updateEditingActions, Qt::UniqueConnection);
}

int PlaylistPane::sourceRow(const QModelIndex &index) const {
    if (!index.isValid())
        return -1;
    return index.model() == filter_ ? filter_->mapToSource(index).row() : index.row();
}

QModelIndex PlaylistPane::sourceIndexForCurrent() const {
    const auto row = model_->currentIndex();
    if (row < 0 || row >= model_->rowCount())
        return {};
    const auto source = model_->index(row);
    return filtering_ ? filter_->mapFromSource(source) : source;
}

void PlaylistPane::selectSourceCurrent() {
    if (auto *selection = view_->selectionModel()) {
        const auto index = sourceIndexForCurrent();
        if (index.isValid()) {
            view_->setCurrentIndex(index);
            return;
        }
        selection->clearSelection();
        selection->clearCurrentIndex();
    }
}

void PlaylistPane::updateItemCount() {
    auto *count = findChild<QLabel *>(QStringLiteral("playlistCount"));
    if (!count)
        return;
    if (filtering_)
        count->setText(tr("%1 / %2 项").arg(filter_->rowCount()).arg(model_->rowCount()));
    else
        count->setText(tr("%1 项").arg(model_->rowCount()));
}

void PlaylistPane::synchronizeCurrent() {
    const auto current = model_->index(model_->currentIndex());
    // Reordering changes row numbers, but not the playing entry's identity.
    // Preserve a separate editing selection until playback changes entries.
    if (playingIndex_ != current) {
        playingIndex_ = current;
        selectSourceCurrent();
    }
    updateEditingActions();
}

void PlaylistPane::updateEditingActions() {
    if (filtering_) {
        remove_->setEnabled(false);
        moveUp_->setEnabled(false);
        moveDown_->setEnabled(false);
        return;
    }
    const auto row = view_->currentIndex().row();
    remove_->setEnabled(row >= 0);
    moveUp_->setEnabled(row > 0);
    moveDown_->setEnabled(row >= 0 && row + 1 < model_->rowCount());
}
