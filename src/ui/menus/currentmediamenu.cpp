#include "ui/menus/currentmediamenu.h"
#include "playback/media/playbacktime.h"
#include "playback/mpv/mpvcontroller.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QUrl>
#include <QWidget>
#include <cmath>

CurrentMediaMenu::CurrentMediaMenu(MpvController &controller, QWidget *window)
    : QObject(window), controller_(controller), menu_(new QMenu(tr("当前媒体"), window)),
      copySource_(new QAction(tr("复制文件路径／网络地址"), this)),
      copyTime_(new QAction(tr("复制当前播放时间"), this)),
      openDirectory_(new QAction(tr("打开所在文件夹"), this)) {
    menu_->setObjectName(QStringLiteral("currentMediaMenu"));
    copySource_->setObjectName(QStringLiteral("copyMediaSourceAction"));
    copySource_->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_C));
    copyTime_->setObjectName(QStringLiteral("copyPlaybackTimeAction"));
    copyTime_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    openDirectory_->setObjectName(QStringLiteral("openMediaDirectoryAction"));
    for (auto *action : {copySource_, copyTime_, openDirectory_}) {
        action->setAutoRepeat(false);
        window->addAction(action);
        menu_->addAction(action);
    }
    connect(copySource_, &QAction::triggered, this, [this] {
        const auto source = controller_.state().path;
        if (!controller_.state().active || source.isEmpty())
            return;
        QApplication::clipboard()->setText(source);
        emit feedback(tr("媒体路径／地址已复制"));
    });
    connect(copyTime_, &QAction::triggered, this, [this] {
        if (!controller_.state().active)
            return;
        const auto time = formatPlaybackTime(controller_.state().position, true);
        if (time.startsWith(QLatin1String("--")))
            return;
        QApplication::clipboard()->setText(time);
        emit feedback(tr("播放时间已复制：%1").arg(time));
    });
    connect(openDirectory_, &QAction::triggered, this, [this] {
        const auto source = controller_.state().path;
        if (!controller_.state().active || !QDir::isAbsolutePath(source))
            return;
        const auto directory = QUrl::fromLocalFile(QFileInfo(source).absolutePath());
        if (!QDesktopServices::openUrl(directory))
            emit errorOccurred(tr("无法打开媒体所在文件夹"));
    });
    connect(&controller_, &MpvController::stateChanged, this,
            &CurrentMediaMenu::updateAvailability);
    updateAvailability();
}

QMenu *CurrentMediaMenu::menu() const { return menu_; }

void CurrentMediaMenu::updateAvailability() {
    const auto &state = controller_.state();
    const bool available = state.active && !state.path.isEmpty();
    copySource_->setEnabled(available);
    copyTime_->setEnabled(available && std::isfinite(state.position) && state.position >= 0);
    openDirectory_->setEnabled(available && QDir::isAbsolutePath(state.path));
}
