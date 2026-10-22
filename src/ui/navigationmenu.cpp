#include "navigationmenu.h"
#include "abloopmenu.h"
#include "actionfeedbacktext.h"
#include "playback/mpvcontroller.h"
#include "playback/playbacktime.h"
#include "timeseekdialog.h"

#include <QAction>
#include <QActionGroup>
#include <QMenu>
#include <QWidget>
#include <cmath>

NavigationMenu::NavigationMenu(MpvController &player, QWidget *window)
    : QObject(window), player_(player), window_(window), menu_(new QMenu(tr("定位"), window)),
      chaptersMenu_(menu_->addMenu(tr("章节"))), chapterGroup_(new QActionGroup(chaptersMenu_)) {
    menu_->setObjectName(QStringLiteral("navigationMenu"));
    chaptersMenu_->setObjectName(QStringLiteral("chaptersMenu"));
    const auto make = [this](const QString &text, const char *name, QKeySequence shortcut) {
        auto *action = menu_->addAction(text);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        window_->addAction(action);
        return action;
    };
    const auto relativeSeek = [this, &make](const QString &text, const char *name,
                                            QKeySequence shortcut, double seconds) {
        auto *action = make(text, name, shortcut);
        seekActions_.append(action);
        connect(action, &QAction::triggered, this, [this, seconds] {
            player_.seekRelative(seconds);
            emit seekFeedback(ActionFeedbackText::seek(seconds));
        });
    };
    menu_->addSeparator();
    relativeSeek(tr("后退 30 秒"), "seekBack30Action", QKeySequence(Qt::SHIFT | Qt::Key_Left), -30);
    relativeSeek(tr("前进 30 秒"), "seekForward30Action", QKeySequence(Qt::SHIFT | Qt::Key_Right),
                 30);
    relativeSeek(tr("后退 5 分钟"), "seekBack300Action",
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Left), -300);
    relativeSeek(tr("前进 5 分钟"), "seekForward300Action",
                 QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Right), 300);
    auto *beginning = make(tr("回到开头"), "seekBeginningAction", QKeySequence(Qt::Key_Home));
    beginning->setAutoRepeat(false);
    seekActions_.append(beginning);
    connect(beginning, &QAction::triggered, this, [this] {
        player_.seekAbsolute(0);
        emit seekFeedback(ActionFeedbackText::seekTo(0));
    });
    menu_->addSeparator();
    seekTime_ = make(tr("跳转到指定时间…"), "seekTimeAction", QKeySequence(Qt::CTRL | Qt::Key_G));
    previous_ =
        make(tr("上一章"), "previousChapterAction", QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    next_ = make(tr("下一章"), "nextChapterAction", QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    menu_->addSeparator();
    previousFrame_ =
        make(tr("上一帧"), "previousFrameAction", QKeySequence(Qt::CTRL | Qt::Key_Left));
    nextFrame_ = make(tr("下一帧"), "nextFrameAction", QKeySequence(Qt::CTRL | Qt::Key_Right));
    // Do not accumulate expensive backward seeks while a key is held down.
    previousFrame_->setAutoRepeat(false);
    nextFrame_->setAutoRepeat(false);
    auto *loop = new AbLoopMenu(player_, window_);
    menu_->addSeparator();
    menu_->addMenu(loop->menu());
    connect(loop, &AbLoopMenu::feedback, this, &NavigationMenu::feedback);
    connect(seekTime_, &QAction::triggered, this, &NavigationMenu::chooseTime);
    connect(previous_, &QAction::triggered, &player_, &MpvController::previousChapter);
    connect(next_, &QAction::triggered, &player_, &MpvController::nextChapter);
    connect(previousFrame_, &QAction::triggered, &player_, &MpvController::stepFrameBackward);
    connect(nextFrame_, &QAction::triggered, &player_, &MpvController::stepFrameForward);
    connect(&player_, &MpvController::chaptersChanged, this, &NavigationMenu::rebuildChapters);
    connect(&player_, &MpvController::stateChanged, this, &NavigationMenu::updateState);
    connect(&player_, &MpvController::chapterNavigationChanged, this, &NavigationMenu::updateState);
    rebuildChapters();
}

QMenu *NavigationMenu::menu() const { return menu_; }

void NavigationMenu::retranslate() {
    menu_->setTitle(tr("定位"));
    chaptersMenu_->setTitle(tr("章节"));
    const auto setText = [this](const char *name, const QString &text) {
        if (auto *action = menu_->findChild<QAction *>(QString::fromLatin1(name)))
            action->setText(text);
    };
    setText("seekBack30Action", tr("后退 30 秒"));
    setText("seekForward30Action", tr("前进 30 秒"));
    setText("seekBack300Action", tr("后退 5 分钟"));
    setText("seekForward300Action", tr("前进 5 分钟"));
    setText("seekBeginningAction", tr("回到开头"));
    setText("seekTimeAction", tr("跳转到指定时间…"));
    setText("previousChapterAction", tr("上一章"));
    setText("nextChapterAction", tr("下一章"));
    setText("previousFrameAction", tr("上一帧"));
    setText("nextFrameAction", tr("下一帧"));
    for (auto *loop : window_->findChildren<AbLoopMenu *>())
        loop->retranslate();
    rebuildChapters();
}

void NavigationMenu::rebuildChapters() {
    chaptersMenu_->clear();
    for (const auto &chapter : player_.chapters()) {
        auto title =
            chapter.title.isEmpty() ? tr("第 %1 章").arg(chapter.index + 1) : chapter.title;
        title.replace(QLatin1Char('&'), QStringLiteral("&&"));
        auto *action =
            chaptersMenu_->addAction(tr("%1  %2  %3")
                                         .arg(chapter.index + 1)
                                         .arg(formatPlaybackTime(chapter.start, true), title));
        action->setObjectName(QStringLiteral("chapterAction%1").arg(chapter.index));
        action->setCheckable(true);
        action->setData(chapter.index);
        chapterGroup_->addAction(action);
        connect(action, &QAction::triggered, this,
                [this, index = chapter.index] { player_.selectChapter(index); });
    }
    if (player_.chapters().isEmpty())
        chaptersMenu_->addAction(tr("当前媒体没有章节"))->setEnabled(false);
    presentedChapter_ = -2;
    updateState();
}

void NavigationMenu::updateState() {
    const auto &state = player_.state();
    const bool seekable = state.active && state.seekable;
    const bool timeSeekable = seekable && std::isfinite(state.duration) && state.duration > 0;
    const auto navigationIndex = player_.chapterNavigationIndex();
    const bool frameForward = player_.canStepFrameForward();
    const bool frameBackward = player_.canStepFrameBackward();
    if (presentedChapter_ == state.currentChapter && presentedSeekable_ == seekable &&
        presentedTimeSeekable_ == timeSeekable && presentedNavigationChapter_ == navigationIndex &&
        presentedFrameForward_ == frameForward && presentedFrameBackward_ == frameBackward)
        return;
    presentedChapter_ = state.currentChapter;
    presentedNavigationChapter_ = navigationIndex;
    presentedSeekable_ = seekable;
    presentedTimeSeekable_ = timeSeekable;
    presentedFrameForward_ = frameForward;
    presentedFrameBackward_ = frameBackward;
    nextFrame_->setEnabled(frameForward);
    previousFrame_->setEnabled(frameBackward);
    seekTime_->setEnabled(timeSeekable);
    for (auto *action : seekActions_)
        action->setEnabled(seekable);
    chaptersMenu_->setEnabled(seekable && !player_.chapters().isEmpty());
    bool hasPrevious = false;
    bool hasNext = false;
    for (auto *action : chapterGroup_->actions()) {
        const auto index = action->data().toInt();
        action->setChecked(index == state.currentChapter);
        hasPrevious |= index == navigationIndex - 1;
        hasNext |= index == navigationIndex + 1;
    }
    previous_->setEnabled(seekable && hasPrevious);
    next_->setEnabled(seekable && hasNext);
}

void NavigationMenu::chooseTime() {
    if (!seekTime_->isEnabled())
        return;
    const auto path = player_.state().path;
    TimeSeekDialog dialog(player_.state().position, player_.state().duration, window_);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const auto seconds = dialog.selectedSeconds();
    const auto &state = player_.state();
    if (!state.active || !state.seekable || path != state.path || !seconds ||
        *seconds > state.duration) {
        emit feedback(tr("媒体已改变，未执行时间跳转。"));
        return;
    }
    player_.seekAbsolute(*seconds);
}
