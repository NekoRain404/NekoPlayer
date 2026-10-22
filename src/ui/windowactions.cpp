#include "actionfeedbacktext.h"
#include "audiodevicemenu.h"
#include "foldermenu.h"
#include "mainwindow.h"
#include "mediainformationdialog.h"
#include "navigationmenu.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playbackoverlay.h"
#include "playlistmodemenu.h"
#include "playlistpane.h"
#include "preferencesdialog.h"
#include "subtitledisplaymenu.h"
#include "trackmenus.h"
#include "tuningmenus.h"
#include "videomenu.h"

#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>
#include <QtGlobal>

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
    openNetworkAction_ =
        make(tr("打开网络地址…"), "openNetworkAction", QKeySequence(Qt::CTRL | Qt::Key_U));
    connect(openNetworkAction_, &QAction::triggered, this, &MainWindow::chooseNetworkUrl);
    openPlaylistAction_ = make(tr("打开播放列表…"), "openPlaylistAction", {});
    connect(openPlaylistAction_, &QAction::triggered, this, &MainWindow::choosePlaylistToOpen);
    savePlaylistAction_ = make(tr("保存播放列表…"), "savePlaylistAction", {});
    connect(savePlaylistAction_, &QAction::triggered, this, &MainWindow::choosePlaylistToSave);
    appendAction_ =
        make(tr("添加文件…"), "appendAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    appendAction_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
    connect(appendAction_, &QAction::triggered, this, &MainWindow::chooseFilesToAppend);
    pauseAction_ = make(tr("暂停"), "pauseAction", QKeySequence(Qt::Key_Space));
    connect(pauseAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playOrPause);
    connect(pauseAction_, &QAction::triggered, this, [this] {
        showActionFeedback(ActionFeedbackText::playback(pauseAction_->text() == tr("暂停")));
    });
    stopAction_ = make(tr("停止"), "stopAction", QKeySequence(Qt::Key_S));
    stopAction_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    connect(stopAction_, &QAction::triggered, controller_.get(), &MpvController::stop);
    connect(stopAction_, &QAction::triggered, this, [this] { showActionFeedback(tr("已停止")); });
    previousAction_ = make(tr("上一个"), "previousAction", QKeySequence(Qt::Key_MediaPrevious));
    previousAction_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    connect(previousAction_, &QAction::triggered, playlist_.get(),
            &PlaylistController::playPrevious);
    connect(previousAction_, &QAction::triggered, this,
            [this] { showActionFeedback(tr("上一个")); });
    nextAction_ = make(tr("下一个"), "nextAction", QKeySequence(Qt::Key_MediaNext));
    nextAction_->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));
    connect(nextAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playNext);
    connect(nextAction_, &QAction::triggered, this, [this] { showActionFeedback(tr("下一个")); });
    clearPlaylistAction_ = make(tr("清空播放列表"), "clearPlaylistAction", {});
    clearPlaylistAction_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    connect(clearPlaylistAction_, &QAction::triggered, this, &MainWindow::clearPlaylist);
    muteAction_ = make(tr("静音"), "muteAction", QKeySequence(Qt::Key_M));
    muteAction_->setCheckable(true);
    connect(muteAction_, &QAction::triggered, controller_.get(), &MpvController::setMuted);
    connect(muteAction_, &QAction::triggered, this,
            [this](bool muted) { showActionFeedback(ActionFeedbackText::mute(muted)); });
    fullscreenAction_ = make(tr("全屏"), "fullscreenAction", QKeySequence(Qt::Key_F));
    fullscreenAction_->setCheckable(true);
    fullscreenAction_->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
    connect(fullscreenAction_, &QAction::triggered, this, &MainWindow::setFullscreen);
    auto *escape = make(tr("退出全屏"), "exitFullscreenAction", QKeySequence(Qt::Key_Escape));
    connect(escape, &QAction::triggered, this, [this] { setFullscreen(false); });
    auto *back = make(tr("后退 5 秒"), "seekBackAction", QKeySequence(Qt::Key_Left));
    auto *forward = make(tr("前进 5 秒"), "seekForwardAction", QKeySequence(Qt::Key_Right));
    connect(back, &QAction::triggered, this, [this] {
        controller_->seekRelative(-5);
        showSeekFeedback(ActionFeedbackText::seek(-5));
    });
    connect(forward, &QAction::triggered, this, [this] {
        controller_->seekRelative(5);
        showSeekFeedback(ActionFeedbackText::seek(5));
    });
    auto *louder = make(tr("增加音量"), "volumeUpAction", QKeySequence(Qt::Key_Up));
    auto *quieter = make(tr("降低音量"), "volumeDownAction", QKeySequence(Qt::Key_Down));
    connect(louder, &QAction::triggered, this, [this] {
        const auto volume = qBound(0.0, controller_->state().volume + 5.0, 100.0);
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    connect(quieter, &QAction::triggered, this, [this] {
        const auto volume = qBound(0.0, controller_->state().volume - 5.0, 100.0);
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    auto *quit = make(tr("退出"), "quitAction", QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *information =
        make(tr("媒体信息…"), "mediaInformationAction", QKeySequence(Qt::CTRL | Qt::Key_F1));
    connect(information, &QAction::triggered, this, [this] {
        auto *dialog = findChild<MediaInformationDialog *>();
        if (!dialog)
            dialog = new MediaInformationDialog(*controller_, this);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    folderMenu_ = new FolderMenu(this, [this] { return mediaDirectory(); });
    connect(folderMenu_, &FolderMenu::filesReady, this,
            [this](const QStringList &paths, bool append) {
                if (append)
                    appendFiles(paths);
                else
                    openFiles(paths);
            });
    connect(folderMenu_, &FolderMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message); });
    connect(folderMenu_, &FolderMenu::errorOccurred, this, &MainWindow::showError);
    connect(stopAction_, &QAction::triggered, folderMenu_, &FolderMenu::cancel);
    connect(previousAction_, &QAction::triggered, folderMenu_, &FolderMenu::cancel);
    connect(nextAction_, &QAction::triggered, folderMenu_, &FolderMenu::cancel);
    // Structural edits supersede a pending folder import, including direct list drag/drop.
    auto *model = playlist_->model();
    connect(model, &PlaylistModel::rowsAboutToBeInserted, folderMenu_, &FolderMenu::cancel);
    connect(model, &PlaylistModel::rowsAboutToBeRemoved, folderMenu_, &FolderMenu::cancel);
    connect(model, &PlaylistModel::rowsAboutToBeMoved, folderMenu_, &FolderMenu::cancel);
    connect(model, &PlaylistModel::modelAboutToBeReset, folderMenu_, &FolderMenu::cancel);
    auto *file = menuBar()->addMenu(tr("文件(&F)"));
    file->addAction(openAction_);
    file->addAction(openNetworkAction_);
    file->addAction(appendAction_);
    file->addMenu(folderMenu_->menu());
    file->addSeparator();
    file->addActions({openPlaylistAction_, savePlaylistAction_});
    file->addSeparator();
    file->addAction(information);
    file->addSeparator();
    file->addAction(quit);
    auto *playback = menuBar()->addMenu(tr("播放(&P)"));
    playback->addActions({previousAction_, pauseAction_, nextAction_, stopAction_});
    playback->addSeparator();
    playback->addActions({back, forward});
    auto *navigation = new NavigationMenu(*controller_, this);
    connect(navigation, &NavigationMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 4000); });
    playback->addMenu(navigation->menu());
    auto *tuning = new TuningMenus(*controller_, this);
    connect(tuning, &TuningMenus::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    playback->addMenu(tuning->speedMenu());
    auto *audio = menuBar()->addMenu(tr("声音(&A)"));
    audio->addActions({muteAction_, louder, quieter});
    auto *subtitleDisplay = new SubtitleDisplayMenu(*controller_, this);
    connect(subtitleDisplay, &SubtitleDisplayMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    trackMenus_ =
        new TrackMenus(*controller_, this, tuning->subtitleDelayMenu(), subtitleDisplay->menu());
    auto *audioDevices = new AudioDeviceMenu(*controller_, this);
    audio->addSeparator();
    audio->addMenu(audioDevices->menu());
    audio->addMenu(trackMenus_->audioMenu());
    audio->addMenu(tuning->audioDelayMenu());
    menuBar()->addMenu(trackMenus_->subtitleMenu());
    auto *video = new VideoMenu(*controller_, this);
    connect(video, &VideoMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    menuBar()->addMenu(video->menu());
    playlistMenu_ = menuBar()->addMenu(tr("播放列表(&L)"));
    playlistMenu_->setObjectName(QStringLiteral("playlistMenu"));
    playlistMenu_->addActions({appendAction_, previousAction_, nextAction_, clearPlaylistAction_});
    playlistMenu_->addMenu(folderMenu_->menu());
    playlistMenu_->addSeparator();
    playlistMenu_->addActions({openPlaylistAction_, savePlaylistAction_});
    auto *modeMenu = new PlaylistModeMenu(*playlist_, this);
    connect(modeMenu, &PlaylistModeMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    playlistMenu_->addMenu(modeMenu->menu());
    auto *view = menuBar()->addMenu(tr("视图(&V)"));
    view->addAction(fullscreenAction_);
    auto *playbackInformation =
        make(tr("显示当前播放信息"), "playbackInformationAction", QKeySequence(Qt::Key_Tab));
    playbackInformation->setCheckable(true);
    playbackInformation->setAutoRepeat(false);
    connect(playbackInformation, &QAction::toggled, this, [this](bool visible) {
        if (playbackOverlay_)
            playbackOverlay_->setInformationVisible(visible);
    });
    view->addAction(playbackInformation);
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
        PreferencesDialog dialog(*controller_, this);
        dialog.selectCategory(PreferencesDialog::Category::Playback);
        dialog.exec();
    });
    settings->addAction(playbackSettings);
    settings->addAction(subtitleDisplay->settingsAction());
    contextMenu_ = new QMenu(this);
    contextMenu_->addActions(
        {openAction_, openNetworkAction_, appendAction_, pauseAction_, stopAction_});
    contextMenu_->addMenu(folderMenu_->menu());
    contextMenu_->addSeparator();
    contextMenu_->addMenu(navigation->menu());
    contextMenu_->addMenu(tuning->speedMenu());
    contextMenu_->addMenu(tuning->audioDelayMenu());
    contextMenu_->addMenu(modeMenu->menu());
    contextMenu_->addActions(
        {previousAction_, nextAction_, showPlaylistAction_, muteAction_, fullscreenAction_});
    contextMenu_->addSeparator();
    contextMenu_->addMenu(audioDevices->menu());
    contextMenu_->addMenu(trackMenus_->audioMenu());
    contextMenu_->addMenu(trackMenus_->subtitleMenu());
    contextMenu_->addMenu(video->menu());
    contextMenu_->addSeparator();
    contextMenu_->addAction(information);
    contextMenu_->addAction(playbackInformation);
    contextMenu_->addAction(playbackSettings);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this,
            [this](const QPoint &point) { contextMenu_->popup(mapToGlobal(point)); });
}
