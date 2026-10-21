#include "mainwindow.h"
#include "navigationmenu.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playbacksettingsdialog.h"
#include "playlistmodemenu.h"
#include "playlistpane.h"
#include "subtitledisplaymenu.h"
#include "trackmenus.h"
#include "tuningmenus.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>

void MainWindow::createActions() {
    const auto make = [this](const QString &text, const char *name, QKeySequence shortcut) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        addAction(action);
        return action;
    };
    openAction_ = make(tr("打开文件…"), "openAction", QKeySequence::Open);
    openAction_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseFile);
    appendAction_ =
        make(tr("添加文件…"), "appendAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    appendAction_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(appendAction_, &QAction::triggered, this, &MainWindow::chooseFilesToAppend);
    pauseAction_ = make(tr("暂停"), "pauseAction", QKeySequence(Qt::Key_Space));
    connect(pauseAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playOrPause);
    stopAction_ = make(tr("停止"), "stopAction", QKeySequence(Qt::Key_S));
    stopAction_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    connect(stopAction_, &QAction::triggered, controller_.get(), &MpvController::stop);
    previousAction_ = make(tr("上一个"), "previousAction", QKeySequence(Qt::Key_MediaPrevious));
    previousAction_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    connect(previousAction_, &QAction::triggered, playlist_.get(),
            &PlaylistController::playPrevious);
    nextAction_ = make(tr("下一个"), "nextAction", QKeySequence(Qt::Key_MediaNext));
    nextAction_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    connect(nextAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playNext);
    clearPlaylistAction_ = make(tr("清空播放列表"), "clearPlaylistAction", {});
    clearPlaylistAction_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(clearPlaylistAction_, &QAction::triggered, this, &MainWindow::clearPlaylist);
    muteAction_ = make(tr("静音"), "muteAction", QKeySequence(Qt::Key_M));
    muteAction_->setCheckable(true);
    connect(muteAction_, &QAction::triggered, controller_.get(), &MpvController::setMuted);
    fullscreenAction_ = make(tr("全屏"), "fullscreenAction", QKeySequence(Qt::Key_F));
    fullscreenAction_->setCheckable(true);
    fullscreenAction_->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    connect(fullscreenAction_, &QAction::triggered, this, &MainWindow::setFullscreen);
    auto *escape = make(tr("退出全屏"), "exitFullscreenAction", QKeySequence(Qt::Key_Escape));
    connect(escape, &QAction::triggered, this, [this] { setFullscreen(false); });
    auto *back = make(tr("后退 5 秒"), "seekBackAction", QKeySequence(Qt::Key_Left));
    auto *forward = make(tr("前进 5 秒"), "seekForwardAction", QKeySequence(Qt::Key_Right));
    connect(back, &QAction::triggered, this, [this] { controller_->seekRelative(-5); });
    connect(forward, &QAction::triggered, this, [this] { controller_->seekRelative(5); });
    auto *louder = make(tr("增加音量"), "volumeUpAction", QKeySequence(Qt::Key_Up));
    auto *quieter = make(tr("降低音量"), "volumeDownAction", QKeySequence(Qt::Key_Down));
    connect(louder, &QAction::triggered, this,
            [this] { controller_->setVolume(controller_->state().volume + 5); });
    connect(quieter, &QAction::triggered, this,
            [this] { controller_->setVolume(controller_->state().volume - 5); });
    auto *quit = make(tr("退出"), "quitAction", QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *file = menuBar()->addMenu(tr("文件(&F)"));
    file->addAction(openAction_);
    file->addAction(appendAction_);
    file->addSeparator();
    file->addAction(quit);
    auto *playback = menuBar()->addMenu(tr("播放(&P)"));
    playback->addActions({previousAction_, pauseAction_, nextAction_, stopAction_});
    playback->addSeparator();
    playback->addActions({back, forward});
    auto *navigation = new NavigationMenu(*controller_, this);
    connect(navigation, &NavigationMenu::feedback, this,
            [this](const QString &message) { statusBar()->showMessage(message, 4000); });
    playback->addMenu(navigation->menu());
    auto *tuning = new TuningMenus(*controller_, this);
    connect(tuning, &TuningMenus::feedback, this,
            [this](const QString &message) { statusBar()->showMessage(message, 2500); });
    playback->addMenu(tuning->speedMenu());
    auto *audio = menuBar()->addMenu(tr("声音(&A)"));
    audio->addActions({muteAction_, louder, quieter});
    auto *subtitleDisplay = new SubtitleDisplayMenu(*controller_, this);
    connect(subtitleDisplay, &SubtitleDisplayMenu::feedback, this,
            [this](const QString &message) { statusBar()->showMessage(message, 2500); });
    trackMenus_ =
        new TrackMenus(*controller_, this, tuning->subtitleDelayMenu(), subtitleDisplay->menu());
    audio->addSeparator();
    audio->addMenu(trackMenus_->audioMenu());
    audio->addMenu(tuning->audioDelayMenu());
    menuBar()->addMenu(trackMenus_->subtitleMenu());
    playlistMenu_ = menuBar()->addMenu(tr("播放列表(&L)"));
    playlistMenu_->setObjectName(QStringLiteral("playlistMenu"));
    playlistMenu_->addActions({appendAction_, previousAction_, nextAction_, clearPlaylistAction_});
    auto *modeMenu = new PlaylistModeMenu(*playlist_, this);
    connect(modeMenu, &PlaylistModeMenu::feedback, this,
            [this](const QString &message) { statusBar()->showMessage(message, 2500); });
    playlistMenu_->addMenu(modeMenu->menu());
    auto *view = menuBar()->addMenu(tr("视图(&V)"));
    view->addAction(fullscreenAction_);
    showPlaylistAction_ =
        make(tr("显示播放列表"), "showPlaylistAction", QKeySequence(Qt::CTRL | Qt::Key_L));
    showPlaylistAction_->setCheckable(true);
    showPlaylistAction_->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    connect(showPlaylistAction_, &QAction::toggled, this,
            [this](bool visible) { playlistPane_->setVisible(visible); });
    view->addAction(showPlaylistAction_);
    auto *settings = menuBar()->addMenu(tr("设置(&S)"));
    auto *playbackSettings =
        make(tr("播放设置…"), "playbackSettingsAction", QKeySequence(Qt::Key_F5));
    connect(playbackSettings, &QAction::triggered, this, [this] {
        PlaybackSettingsDialog dialog(*controller_, this);
        dialog.exec();
    });
    settings->addAction(playbackSettings);
    settings->addAction(subtitleDisplay->settingsAction());
    contextMenu_ = new QMenu(this);
    contextMenu_->addActions({openAction_, appendAction_, pauseAction_, stopAction_});
    contextMenu_->addSeparator();
    contextMenu_->addMenu(navigation->menu());
    contextMenu_->addMenu(tuning->speedMenu());
    contextMenu_->addMenu(tuning->audioDelayMenu());
    contextMenu_->addMenu(modeMenu->menu());
    contextMenu_->addActions(
        {previousAction_, nextAction_, showPlaylistAction_, muteAction_, fullscreenAction_});
    contextMenu_->addSeparator();
    contextMenu_->addMenu(trackMenus_->audioMenu());
    contextMenu_->addMenu(trackMenus_->subtitleMenu());
    contextMenu_->addSeparator();
    contextMenu_->addAction(playbackSettings);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &point) { contextMenu_->popup(mapToGlobal(point)); });
}
