#include "playlistpane.h"
#include "appearanceicons.h"
#include "appearancetokens.h"
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
#include <QMouseEvent>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr auto selectionModifiers =
    Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier;

QToolButton *actionButton(QAction *action, QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    button->setMinimumSize(tokens.transportHitTarget, tokens.rowHeight);
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
      view_(new QListView(this)), emptyHint_(new QLabel(this)),
      remove_(new QAction(tr("从列表移除"), this)), moveUp_(new QAction(tr("上移所选项"), this)),
      moveDown_(new QAction(tr("下移所选项"), this)),
      focusSearchAction_(new QAction(tr("搜索播放列表"), this)),
      clearSearchAction_(new QAction(tr("清除播放列表搜索"), this)),
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
    emptyHint_->setObjectName(QStringLiteral("playlistEmptyHint"));
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setWordWrap(true);
    emptyHint_->setFocusPolicy(Qt::NoFocus);
    emptyHint_->setAccessibleName(tr("播放列表提示"));
    view_->setObjectName(QStringLiteral("playlistView"));
    view_->setModel(model_);
    view_->setTextElideMode(Qt::ElideMiddle);
    view_->setItemDelegate(new PlaylistItemDelegate(view_));
    view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    view_->setDragDropMode(QAbstractItemView::InternalMove);
    view_->setDefaultDropAction(Qt::MoveAction);
    view_->setDragDropOverwriteMode(false);
    view_->viewport()->installEventFilter(this);
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
    connect(remove_, &QAction::triggered, this, [this] {
        auto rows = selectedSourceRows();
        if (rows.isEmpty()) {
            const auto row = sourceRow(view_->currentIndex());
            if (row >= 0)
                rows.append(row);
        }
        if (!rows.isEmpty())
            queue_.removeRowsAt(rows);
    });
    connect(moveUp_, &QAction::triggered, this, [this] {
        const auto row = singleSelectedSourceRow();
        if (row > 0)
            queue_.moveItem(row, row - 1);
    });
    connect(moveDown_, &QAction::triggered, this, [this] {
        const auto row = singleSelectedSourceRow();
        if (row >= 0 && row + 1 < model_->rowCount())
            queue_.moveItem(row, row + 1);
    });
    connectSelectionModel();
    connect(search_, &QLineEdit::textChanged, this, &PlaylistPane::applyFilter);
    focusSearchAction_->setObjectName(QStringLiteral("focusPlaylistSearchAction"));
    focusSearchAction_->setShortcut(QKeySequence::Find);
    focusSearchAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    clearSearchAction_->setObjectName(QStringLiteral("clearPlaylistSearchAction"));
    clearSearchAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    clearSearchAction_->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(focusSearchAction_);
    addAction(clearSearchAction_);
    connect(focusSearchAction_, &QAction::triggered, this, [this] {
        search_->setFocus(Qt::ShortcutFocusReason);
        search_->selectAll();
    });
    connect(clearSearchAction_, &QAction::triggered, search_, &QLineEdit::clear);
    connect(model_, &PlaylistModel::currentIndexChanged, this, &PlaylistPane::synchronizeCurrent);
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::rowsMoved, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistPane::updateEditingActions);
    connect(model_, &PlaylistModel::modelReset, this, [this] { search_->clear(); });
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateEmptyHint);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateEmptyHint);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistPane::updateEmptyHint);
    connect(model_, &PlaylistModel::rowsInserted, this, &PlaylistPane::updateItemCount);
    connect(model_, &PlaylistModel::rowsRemoved, this, &PlaylistPane::updateItemCount);
    connect(model_, &PlaylistModel::modelReset, this, &PlaylistPane::updateItemCount);
    view_->setAlternatingRowColors(false);
    view_->setUniformItemSizes(true);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *layout = new QVBoxLayout(this);
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    layout->setContentsMargins(tokens.spaceUnit * 3, tokens.spaceUnit * 3, tokens.spaceUnit * 3,
                               tokens.spaceUnit * 3);
    layout->setSpacing(tokens.spaceUnit * 2);
    search_->setMinimumHeight(tokens.controlHeight);
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("playlistHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(tokens.spaceUnit * 2, tokens.spaceUnit, tokens.spaceUnit * 2,
                                     tokens.spaceUnit);
    headerLayout->setSpacing(tokens.spaceUnit * 2);
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
    layout->addWidget(emptyHint_);
    layout->addWidget(view_, 1);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(tokens.spaceUnit * 2);
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
        // Consume the recorded modifiers so a later synthetic click cannot be
        // suppressed by stale state.
        const auto modifiers = lastClickModifiers_;
        lastClickModifiers_ = Qt::NoModifier;
        if (modifiers & selectionModifiers) {
            singleClickTimer_->stop();
            pendingClickIndex_ = QPersistentModelIndex{};
            return;
        }
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
    updateEmptyHint();
}

QListView *PlaylistPane::view() const { return view_; }

QList<QAction *> PlaylistPane::editingActions() const { return {remove_, moveUp_, moveDown_}; }

QList<QAction *> PlaylistPane::shortcutActions() const {
    return {focusSearchAction_, clearSearchAction_};
}

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
    focusSearchAction_->setText(tr("搜索播放列表"));
    clearSearchAction_->setText(tr("清除播放列表搜索"));
    if (auto *title = findChild<QLabel *>(QStringLiteral("playlistTitle")))
        title->setText(tr("播放列表"));
    search_->setPlaceholderText(tr("搜索播放列表"));
    search_->setAccessibleName(tr("搜索播放列表"));
    view_->setAccessibleName(tr("播放列表"));
    view_->setToolTip(tr("双击播放；可拖动排序"));
    emptyHint_->setAccessibleName(tr("播放列表提示"));
    updateItemCount();
    updateEmptyHint();
}

bool PlaylistPane::eventFilter(QObject *watched, QEvent *event) {
    if (watched == view_->viewport()) {
        switch (event->type()) {
        case QEvent::MouseButtonPress: {
            const auto modifiers = static_cast<QMouseEvent *>(event)->modifiers();
            lastClickModifiers_ = modifiers;
            // A selection gesture must cancel a pending click-to-play before
            // the release, so holding a modifier down cannot start playback.
            if ((modifiers & selectionModifiers) && singleClickTimer_->isActive()) {
                singleClickTimer_->stop();
                pendingClickIndex_ = QPersistentModelIndex{};
            }
            break;
        }
        case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick:
            lastClickModifiers_ = static_cast<QMouseEvent *>(event)->modifiers();
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
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
    updateEmptyHint();
}

void PlaylistPane::connectSelectionModel() {
    if (view_->selectionModel()) {
        connect(view_->selectionModel(), &QItemSelectionModel::currentChanged, this,
                &PlaylistPane::updateEditingActions, Qt::UniqueConnection);
        connect(view_->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                &PlaylistPane::updateEditingActions, Qt::UniqueConnection);
    }
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
            selection->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
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

QList<int> PlaylistPane::selectedSourceRows() const {
    QList<int> rows;
    const auto *selection = view_->selectionModel();
    if (!selection)
        return rows;
    const auto indexes = selection->selectedRows();
    rows.reserve(indexes.size());
    for (const auto &index : indexes) {
        const auto row = sourceRow(index);
        if (row >= 0)
            rows.append(row);
    }
    return rows;
}

int PlaylistPane::singleSelectedSourceRow() const {
    const auto rows = selectedSourceRows();
    return rows.size() == 1 ? rows.first() : -1;
}

void PlaylistPane::updateEditingActions() {
    if (filtering_) {
        remove_->setEnabled(false);
        moveUp_->setEnabled(false);
        moveDown_->setEnabled(false);
        return;
    }
    const auto rows = selectedSourceRows();
    remove_->setEnabled(!rows.isEmpty() || view_->currentIndex().isValid());
    const auto single = rows.size() == 1 ? rows.first() : -1;
    moveUp_->setEnabled(single > 0);
    moveDown_->setEnabled(single >= 0 && single + 1 < model_->rowCount());
}

void PlaylistPane::updateEmptyHint() {
    // An empty queue takes precedence over the filter: there is nothing to search.
    if (model_->rowCount() == 0) {
        emptyHint_->setText(tr("播放列表为空\n打开文件或文件夹，或拖入媒体"));
        emptyHint_->show();
        return;
    }
    if (filtering_ && filter_->rowCount() == 0) {
        emptyHint_->setText(tr("没有匹配项"));
        emptyHint_->show();
        return;
    }
    emptyHint_->hide();
}
