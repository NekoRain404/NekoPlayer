#include "playlistpane.h"

#include "playback/playlistmodel.h"

#include <QAction>
#include <QHBoxLayout>
#include <QListView>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QToolButton *actionButton(QAction *action, QWidget *parent) {
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(true);
    button->setIconSize(QSize(18, 18));
    return button;
}
} // namespace

PlaylistPane::PlaylistPane(PlaylistModel *model, QAction *append, QAction *previous, QAction *next,
                           QAction *clear, QWidget *parent)
    : QWidget(parent), view_(new QListView(this)) {
    setObjectName(QStringLiteral("playlistPane"));
    setMinimumWidth(240);
    view_->setObjectName(QStringLiteral("playlistView"));
    view_->setModel(model);
    view_->setAlternatingRowColors(true);
    view_->setUniformItemSizes(true);
    view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);
    layout->addWidget(view_, 1);
    auto *buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->addWidget(actionButton(previous, this));
    buttons->addWidget(actionButton(next, this));
    buttons->addStretch();
    buttons->addWidget(actionButton(append, this));
    buttons->addWidget(actionButton(clear, this));
    layout->addLayout(buttons);
    connect(view_, &QListView::activated, this,
            [this](const QModelIndex &index) { emit playRequested(index.row()); });
}

QListView *PlaylistPane::view() const { return view_; }
