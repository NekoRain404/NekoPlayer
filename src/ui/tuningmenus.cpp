#include "tuningmenus.h"
#include "playback/mpvcontroller.h"

#include <QAction>
#include <QActionGroup>
#include <QInputDialog>
#include <QMenu>
#include <QWidget>
#include <cmath>

namespace {
QAction *addAction(QMenu *menu, QWidget *window, const QString &text, const QString &name,
                   QKeySequence shortcut, const std::function<void()> &invoke) {
    auto *action = menu->addAction(text);
    action->setObjectName(name);
    action->setShortcut(shortcut);
    window->addAction(action);
    QObject::connect(action, &QAction::triggered, window, invoke);
    return action;
}
} // namespace

TuningMenus::TuningMenus(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), window_(window), speedMenu_(new QMenu(window)),
      audioDelayMenu_(new QMenu(window)), subtitleDelayMenu_(new QMenu(window)),
      speedGroup_(new QActionGroup(speedMenu_)) {
    speedMenu_->setObjectName(QStringLiteral("speedMenu"));
    audioDelayMenu_->setObjectName(QStringLiteral("audioDelayMenu"));
    subtitleDelayMenu_->setObjectName(QStringLiteral("subtitleDelayMenu"));
    createSpeedMenu();
    createDelayMenu(audioDelayMenu_, true);
    createDelayMenu(subtitleDelayMenu_, false);
    connect(&controller_, &MpvController::tuningChanged, this, &TuningMenus::update);
    update();
}

QMenu *TuningMenus::speedMenu() const { return speedMenu_; }
QMenu *TuningMenus::audioDelayMenu() const { return audioDelayMenu_; }
QMenu *TuningMenus::subtitleDelayMenu() const { return subtitleDelayMenu_; }

void TuningMenus::retranslate() {
    const auto speedActions = speedGroup_->actions();
    const std::array speeds{0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0};
    for (qsizetype index = 0;
         index < static_cast<qsizetype>(speeds.size()) && index < speedActions.size(); ++index)
        speedActions.at(index)->setText(tr("%1 倍").arg(speeds.at(index), 0, 'f', 2));
    const auto setText = [](QMenu *menu, const char *name, const QString &text) {
        if (auto *action = menu->findChild<QAction *>(QString::fromLatin1(name)))
            action->setText(text);
    };
    setText(speedMenu_, "speedDownAction", tr("减慢 0.1 倍"));
    setText(speedMenu_, "speedUpAction", tr("加快 0.1 倍"));
    setText(speedMenu_, "speedResetAction", tr("正常速度"));
    setText(speedMenu_, "customSpeedAction", tr("自定义速度…"));
    setText(audioDelayMenu_, "audioEarlierAction", tr("提前 %1 毫秒").arg(50));
    setText(audioDelayMenu_, "audioLaterAction", tr("延后 %1 毫秒").arg(50));
    setText(audioDelayMenu_, "audioDelayResetAction", tr("延迟归零"));
    setText(audioDelayMenu_, "audioCustomDelayAction", tr("自定义延迟…"));
    setText(subtitleDelayMenu_, "subtitleEarlierAction", tr("提前 %1 毫秒").arg(100));
    setText(subtitleDelayMenu_, "subtitleLaterAction", tr("延后 %1 毫秒").arg(100));
    setText(subtitleDelayMenu_, "subtitleDelayResetAction", tr("延迟归零"));
    setText(subtitleDelayMenu_, "subtitleCustomDelayAction", tr("自定义延迟…"));
    update();
}

void TuningMenus::announceAfterChange() { announce_ = true; }

void TuningMenus::createSpeedMenu() {
    for (const auto speed : {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0}) {
        auto *action = speedMenu_->addAction(tr("%1 倍").arg(speed, 0, 'f', 2));
        action->setCheckable(true);
        action->setData(speed);
        speedGroup_->addAction(action);
        connect(action, &QAction::triggered, this, [this, speed] {
            announceAfterChange();
            controller_.setPlaybackSpeed(speed);
        });
    }
    speedMenu_->addSeparator();
    addAction(speedMenu_, window_, tr("减慢 0.1 倍"), QStringLiteral("speedDownAction"),
              QKeySequence(Qt::Key_BracketLeft), [this] {
                  announceAfterChange();
                  controller_.adjustPlaybackSpeed(-0.1);
              });
    addAction(speedMenu_, window_, tr("加快 0.1 倍"), QStringLiteral("speedUpAction"),
              QKeySequence(Qt::Key_BracketRight), [this] {
                  announceAfterChange();
                  controller_.adjustPlaybackSpeed(0.1);
              });
    addAction(speedMenu_, window_, tr("正常速度"), QStringLiteral("speedResetAction"),
              QKeySequence(Qt::Key_Backspace), [this] {
                  announceAfterChange();
                  controller_.setPlaybackSpeed(1.0);
              });
    addAction(speedMenu_, window_, tr("自定义速度…"), QStringLiteral("customSpeedAction"), {},
              [this] { chooseValue(true); });
}

void TuningMenus::createDelayMenu(QMenu *menu, bool audio) {
    const auto prefix = audio ? QStringLiteral("audio") : QStringLiteral("subtitle");
    const auto modifier = audio ? Qt::SHIFT : Qt::KeyboardModifiers{};
    const double step = audio ? 0.05 : 0.1;
    const auto adjust = [this, audio](double seconds) {
        announceAfterChange();
        if (audio)
            controller_.adjustAudioDelay(seconds);
        else
            controller_.adjustSubtitleDelay(seconds);
    };
    addAction(menu, window_, tr("提前 %1 毫秒").arg(qRound(step * 1000)),
              prefix + QStringLiteral("EarlierAction"), QKeySequence(modifier | Qt::Key_Comma),
              [adjust, step] { adjust(-step); });
    addAction(menu, window_, tr("延后 %1 毫秒").arg(qRound(step * 1000)),
              prefix + QStringLiteral("LaterAction"), QKeySequence(modifier | Qt::Key_Period),
              [adjust, step] { adjust(step); });
    addAction(menu, window_, tr("延迟归零"), prefix + QStringLiteral("DelayResetAction"),
              QKeySequence(modifier | Qt::Key_Slash), [this, audio] {
                  announceAfterChange();
                  if (audio)
                      controller_.setAudioDelay(0.0);
                  else
                      controller_.setSubtitleDelay(0.0);
              });
    addAction(menu, window_, tr("自定义延迟…"), prefix + QStringLiteral("CustomDelayAction"), {},
              [this, audio] { chooseValue(false, audio); });
}

void TuningMenus::chooseValue(bool speed, bool audio) {
    const auto &state = controller_.state();
    const auto title = speed ? tr("播放速度") : (audio ? tr("音频延迟") : tr("字幕延迟"));
    const auto label =
        speed ? tr("播放倍率（0.25–4.00，保持音调）") : tr("延迟秒数（正值延后，负值提前）");
    const auto current =
        speed ? state.playbackSpeed : (audio ? state.audioDelay : state.subtitleDelay);
    bool accepted = false;
    const auto value = QInputDialog::getDouble(
        window_, title, label, current, speed ? 0.25 : (audio ? -10.0 : -600.0),
        speed ? 4.0 : (audio ? 10.0 : 600.0), speed ? 2 : 3, &accepted);
    if (!accepted)
        return;
    announceAfterChange();
    if (speed)
        controller_.setPlaybackSpeed(value);
    else if (audio)
        controller_.setAudioDelay(value);
    else
        controller_.setSubtitleDelay(value);
}

void TuningMenus::update() {
    const auto &state = controller_.state();
    speedMenu_->setTitle(tr("播放速度 · %1 倍").arg(state.playbackSpeed, 0, 'f', 2));
    audioDelayMenu_->setTitle(tr("音频延迟 · %1 秒").arg(state.audioDelay, 0, 'f', 3));
    subtitleDelayMenu_->setTitle(tr("字幕延迟 · %1 秒").arg(state.subtitleDelay, 0, 'f', 3));
    for (auto *action : speedGroup_->actions())
        action->setChecked(std::abs(action->data().toDouble() - state.playbackSpeed) < 0.000001);
    for (auto *menu : {speedMenu_, audioDelayMenu_, subtitleDelayMenu_})
        menu->setEnabled(controller_.isReady());
    if (announce_)
        emit feedback(tr("速度 %1 倍 · 音频 %2 秒 · 字幕 %3 秒")
                          .arg(state.playbackSpeed, 0, 'f', 2)
                          .arg(state.audioDelay, 0, 'f', 3)
                          .arg(state.subtitleDelay, 0, 'f', 3));
}
