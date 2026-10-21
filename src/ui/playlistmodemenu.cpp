#include "playlistmodemenu.h"
#include "playback/playlistcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QKeySequence>
#include <QMenu>
#include <QWidget>
#include <array>

PlaylistModeMenu::PlaylistModeMenu(PlaylistController &queue, QWidget *window)
    : QObject(window), queue_(queue), menu_(new QMenu(window)), group_(new QActionGroup(menu_)) {
    menu_->setObjectName(QStringLiteral("playlistModeMenu"));
    using Mode = PlaylistController::RepeatMode;
    const auto modes = std::array{Mode::Off, Mode::One, Mode::All};
    const auto labels = std::array{tr("不循环"), tr("单曲循环"), tr("列表循环")};
    const auto names = std::array{"repeatOffAction", "repeatOneAction", "repeatAllAction"};
    for (size_t i = 0; i < modes.size(); ++i) {
        auto *action = menu_->addAction(labels[i]);
        action->setObjectName(QString::fromLatin1(names[i]));
        action->setCheckable(true);
        action->setData(QVariant::fromValue(modes[i]));
        group_->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, mode = modes[i]] { queue_.setRepeatMode(mode); });
    }
    menu_->addSeparator();
    shuffleAction_ = menu_->addAction(tr("随机播放"));
    shuffleAction_->setObjectName(QStringLiteral("shuffleAction"));
    shuffleAction_->setCheckable(true);
    shuffleAction_->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_R));
    window->addAction(shuffleAction_);
    connect(shuffleAction_, &QAction::triggered, &queue_, &PlaylistController::setShuffleEnabled);
    connect(&queue_, &PlaylistController::shuffleEnabledChanged, this, [this] {
        update();
        emit feedback(menu_->title());
    });
    menu_->addSeparator();
    auto *cycle = menu_->addAction(tr("切换播放模式"));
    cycle->setObjectName(QStringLiteral("cycleRepeatAction"));
    cycle->setShortcut(QKeySequence(Qt::Key_R));
    window->addAction(cycle);
    connect(cycle, &QAction::triggered, &queue_, &PlaylistController::cycleRepeatMode);
    connect(&queue_, &PlaylistController::repeatModeChanged, this, [this] {
        update();
        emit feedback(menu_->title());
    });
    update();
}

QMenu *PlaylistModeMenu::menu() const { return menu_; }

void PlaylistModeMenu::update() {
    shuffleAction_->setChecked(queue_.shuffleEnabled());
    for (auto *action : group_->actions()) {
        const bool selected =
            action->data().value<PlaylistController::RepeatMode>() == queue_.repeatMode();
        action->setChecked(selected);
        if (selected)
            menu_->setTitle(tr("播放模式 · %1").arg(action->text()));
    }
    if (queue_.shuffleEnabled())
        menu_->setTitle(tr("%1 · %2").arg(menu_->title(), shuffleAction_->text()));
}
