#include "playback/render/videowidget.h"
#include "desktop/mprisplayer.h"
#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "ui/dialogs/quickactionsdialog.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QKeySequence>
#include <QMenu>
#include <QSystemTrayIcon>

void MainWindow::createDesktopActions() {
    const auto make = [this](const QString &text, const char *name, QKeySequence shortcut) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        action->setToolTip(text);
        action->setStatusTip(text);
        addAction(action);
        return action;
    };
    mediaPlayAction_ =
        make(tr("播放（媒体键）"), "mediaPlayAction", QKeySequence(Qt::Key_MediaPlay));
    mediaPauseAction_ =
        make(tr("暂停（媒体键）"), "mediaPauseAction", QKeySequence(Qt::Key_MediaPause));
    mediaToggleAction_ = make(tr("播放/暂停（媒体键）"), "mediaToggleAction",
                              QKeySequence(Qt::Key_MediaTogglePlayPause));
    mediaStopAction_ =
        make(tr("停止（媒体键）"), "mediaStopAction", QKeySequence(Qt::Key_MediaStop));
    connect(mediaPlayAction_, &QAction::triggered, this, [this] {
        if (controller_->state().active)
            controller_->setPaused(false);
        else
            playlist_->playOrPause();
    });
    connect(mediaPauseAction_, &QAction::triggered, this, [this] {
        if (controller_->state().active)
            controller_->setPaused(true);
    });
    connect(mediaToggleAction_, &QAction::triggered, playlist_.get(),
            &PlaylistController::playOrPause);
    connect(mediaStopAction_, &QAction::triggered, controller_.get(), &MpvController::stop);
}

void MainWindow::setupDesktopIntegration() {
    setupTrayIcon();
    new QuickActionsDialog(this, this);
    if (auto *quickActions = findChild<QAction *>(QStringLiteral("quickActionsAction")))
        helpMenu_->addAction(quickActions);
    mpris_ = new MprisPlayer(*controller_, *playlist_, this);
    mpris_->setWindow(this);
    connect(mpris_, &MprisPlayer::openUriRequested, this, &MainWindow::openFile);
    connect(mpris_, &MprisPlayer::raiseRequested, this, [this] {
        showNormal();
        raise();
        activateWindow();
    });
    connect(mpris_, &MprisPlayer::quitRequested, this, &QWidget::close);
}

void MainWindow::setupTrayIcon() {
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    auto *menu = new QMenu(this);
    menu->addAction(pauseAction_);
    menu->addAction(previousAction_);
    menu->addAction(nextAction_);
    menu->addSeparator();
    auto *toggle = menu->addAction(tr("显示/隐藏窗口"));
    connect(toggle, &QAction::triggered, this, [this] {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            raise();
            activateWindow();
        }
    });
    menu->addSeparator();
    menu->addAction(quitAction_);
    trayIcon_ = new QSystemTrayIcon(windowIcon().isNull() ? QApplication::windowIcon()
                                                         : windowIcon(),
                                   this);
    trayIcon_->setObjectName(QStringLiteral("trayIcon"));
    trayIcon_->setToolTip(windowTitle());
    trayIcon_->setContextMenu(menu);
    connect(trayIcon_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason != QSystemTrayIcon::Trigger &&
                    reason != QSystemTrayIcon::DoubleClick)
                    return;
                showNormal();
                raise();
                activateWindow();
            });
    trayIcon_->show();
}

void MainWindow::updateDesktopState() {
    if (mediaPlayAction_) {
        const bool hasMedia =
            video_->isReady() && (controller_->state().active || playlist_->model()->rowCount() > 0);
        mediaPlayAction_->setEnabled(hasMedia);
        mediaPauseAction_->setEnabled(controller_->state().active);
        mediaToggleAction_->setEnabled(hasMedia);
        mediaStopAction_->setEnabled(controller_->state().active);
    }
    if (trayIcon_)
        trayIcon_->setToolTip(windowTitle());
}
