#include "ui/menus/subtitledisplaymenu.h"
#include "playback/mpv/mpvcontroller.h"
#include "ui/dialogs/subtitlestyledialog.h"

#include <QAction>
#include <QMenu>
#include <QWidget>
#include <utility>

SubtitleDisplayMenu::SubtitleDisplayMenu(
    MpvController &controller, QWidget *window,
    std::function<void(PreferencesDialog::Category)> showPreferences)
    : QObject(window), controller_(controller), menu_(new QMenu(tr("字幕显示"), window)),
      showPreferences_(std::move(showPreferences)) {
    menu_->setObjectName(QStringLiteral("subtitleDisplayMenu"));
    const auto make = [this, window](const QString &text, const char *name, QKeySequence shortcut) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        window->addAction(action);
        menu_->addAction(action);
        return action;
    };
    visible_ = make(tr("显示字幕"), "subtitleVisibleAction", QKeySequence(Qt::Key_V));
    visible_->setCheckable(true);
    connect(visible_, &QAction::triggered, &controller_, &MpvController::setSubtitleVisible);
    menu_->addSeparator();
    auto *larger =
        make(tr("放大字幕"), "subtitleScaleUpAction", QKeySequence(Qt::ALT | Qt::Key_PageUp));
    auto *smaller =
        make(tr("缩小字幕"), "subtitleScaleDownAction", QKeySequence(Qt::ALT | Qt::Key_PageDown));
    auto *up = make(tr("上移字幕"), "subtitleMoveUpAction", QKeySequence(Qt::ALT | Qt::Key_Up));
    auto *down =
        make(tr("下移字幕"), "subtitleMoveDownAction", QKeySequence(Qt::ALT | Qt::Key_Down));
    connect(larger, &QAction::triggered, this, [this] { controller_.adjustSubtitleScale(0.1); });
    connect(smaller, &QAction::triggered, this, [this] { controller_.adjustSubtitleScale(-0.1); });
    connect(up, &QAction::triggered, this, [this] { controller_.adjustSubtitlePosition(-2); });
    connect(down, &QAction::triggered, this, [this] { controller_.adjustSubtitlePosition(2); });
    menu_->addSeparator();
    auto *style = make(tr("字幕样式…"), "subtitleStyleAction", {});
    connect(style, &QAction::triggered, this, [this, window] {
        SubtitleStyleDialog dialog(controller_, window);
        dialog.exec();
    });
    menu_->addSeparator();
    settings_ = make(tr("字幕显示设置…"), "subtitleSettingsAction", QKeySequence(Qt::Key_F6));
    connect(settings_, &QAction::triggered, this, [this] {
        if (showPreferences_)
            showPreferences_(PreferencesDialog::Category::Subtitles);
    });
    connect(&controller_, &MpvController::subtitlePreferencesChanged, this, [this] {
        update();
        const auto &preferences = controller_.subtitlePreferences();
        emit feedback(tr("字幕%1 · %2 倍 · 位置 %3%")
                          .arg(preferences.visible ? tr("显示") : tr("隐藏"))
                          .arg(preferences.scale, 0, 'f', 2)
                          .arg(preferences.position, 0, 'f', 1));
    });
    update();
}

QMenu *SubtitleDisplayMenu::menu() const { return menu_; }
QAction *SubtitleDisplayMenu::settingsAction() const { return settings_; }

void SubtitleDisplayMenu::retranslate() {
    menu_->setTitle(tr("字幕显示"));
    const auto setText = [this](const char *name, const QString &text) {
        if (auto *action = findChild<QAction *>(QString::fromLatin1(name)))
            action->setText(text);
    };
    setText("subtitleVisibleAction", tr("显示字幕"));
    setText("subtitleScaleUpAction", tr("放大字幕"));
    setText("subtitleScaleDownAction", tr("缩小字幕"));
    setText("subtitleMoveUpAction", tr("上移字幕"));
    setText("subtitleMoveDownAction", tr("下移字幕"));
    setText("subtitleStyleAction", tr("字幕样式…"));
    setText("subtitleSettingsAction", tr("字幕显示设置…"));
}

void SubtitleDisplayMenu::update() {
    visible_->setChecked(controller_.subtitlePreferences().visible);
    for (auto *action : {visible_, settings_})
        action->setEnabled(controller_.isReady());
    menu_->setEnabled(controller_.isReady());
}
