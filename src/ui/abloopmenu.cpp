#include "abloopmenu.h"
#include "playback/mpvcontroller.h"
#include "playback/playbacktime.h"

#include <QAction>
#include <QMenu>
#include <QWidget>
#include <cmath>

AbLoopMenu::AbLoopMenu(MpvController &player, QWidget *window)
    : QObject(window), player_(player), menu_(new QMenu(window)) {
    menu_->setObjectName(QStringLiteral("abLoopMenu"));
    const auto make = [this, window](const QString &text, const char *name, Qt::Key key) {
        auto *action = menu_->addAction(text);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(QKeySequence(Qt::CTRL | key));
        window->addAction(action);
        return action;
    };
    start_ = make(tr("将当前位置设为起点 A"), "loopStartAction", Qt::Key_BracketLeft);
    end_ = make(tr("将当前位置设为终点 B"), "loopEndAction", Qt::Key_BracketRight);
    menu_->addSeparator();
    clear_ = make(tr("清除 A–B 区间"), "clearAbLoopAction", Qt::Key_Backslash);
    connect(start_, &QAction::triggered, this, [this] {
        if (player_.setLoopStart(player_.state().position))
            emit feedback(menu_->title());
        else
            emit feedback(tr("起点必须早于终点和媒体结尾；可先清除原区间。"));
    });
    connect(end_, &QAction::triggered, this, [this] {
        if (player_.setLoopEnd(player_.state().position))
            emit feedback(menu_->title());
        else
            emit feedback(tr("请先设置起点，终点必须晚于起点。"));
    });
    connect(clear_, &QAction::triggered, this, [this] {
        if (player_.clearAbLoop())
            emit feedback(tr("已清除 A–B 区间"));
    });
    connect(&player_, &MpvController::abLoopChanged, this, &AbLoopMenu::updatePresentation);
    connect(&player_, &MpvController::stateChanged, this, &AbLoopMenu::updateAvailability);
    updatePresentation();
}

QMenu *AbLoopMenu::menu() const { return menu_; }

void AbLoopMenu::updateAvailability() {
    const auto &state = player_.state();
    const bool available =
        state.active && state.seekable && std::isfinite(state.duration) && state.duration > 0;
    if (available_ == available)
        return;
    available_ = available;
    start_->setEnabled(available);
    end_->setEnabled(available && player_.abLoop().start.has_value());
}

void AbLoopMenu::updatePresentation() {
    const auto &loop = player_.abLoop();
    const auto start = loop.start ? formatPlaybackTime(*loop.start, true) : tr("未设置");
    const auto end = loop.end ? formatPlaybackTime(*loop.end, true) : tr("未设置");
    menu_->setTitle(tr("A–B 区间循环 · %1 → %2").arg(start, end));
    start_->setEnabled(false);
    end_->setEnabled(false);
    clear_->setEnabled(loop.start.has_value() || loop.end.has_value());
    available_ = false;
    updateAvailability();
}
