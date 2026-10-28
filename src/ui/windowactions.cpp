#include "abloopmenu.h"
#include "actionfeedbacktext.h"
#include "appearanceicons.h"
#include "appearancemenu.h"
#include "appearancepreferences.h"
#include "audiodevicemenu.h"
#include "currentmediamenu.h"
#include "foldermenu.h"
#include "languagemanager.h"
#include "librarydialog.h"
#include "mainwindow.h"
#include "mediainformationdialog.h"
#include "navigationmenu.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playbackoverlay.h"
#include "playlistmaintenancemenu.h"
#include "playlistmodemenu.h"
#include "playlistpane.h"
#include "preferencescoordinator.h"
#include "preferencesdialog.h"
#include "recentmediamenu.h"
#include "session/sessionmanager.h"
#include "shortcutsdialog.h"
#include "subtitledisplaymenu.h"
#include "trackmenus.h"
#include "tuningmenus.h"
#include "videomenu.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QtGlobal>
#include <utility>

void MainWindow::createActions() {
    const auto make = [this](const QString &text, const char *name, QKeySequence shortcut) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        action->setToolTip(text);
        action->setStatusTip(text);
        addAction(action);
        return action;
    };
    openAction_ = make(tr("打开文件…"), "openAction", QKeySequence::Open);
    setAppearanceIcon(openAction_, AppearanceIcons::Symbol::Open);
    connect(openAction_, &QAction::triggered, this, &MainWindow::chooseFile);
    openNetworkAction_ =
        make(tr("打开网络地址…"), "openNetworkAction", QKeySequence(Qt::CTRL | Qt::Key_U));
    setAppearanceIcon(openNetworkAction_, AppearanceIcons::Symbol::Network);
    connect(openNetworkAction_, &QAction::triggered, this, &MainWindow::chooseNetworkUrl);
    openPlaylistAction_ = make(tr("打开播放列表…"), "openPlaylistAction", {});
    setAppearanceIcon(openPlaylistAction_, AppearanceIcons::Symbol::Playlist);
    connect(openPlaylistAction_, &QAction::triggered, this, &MainWindow::choosePlaylistToOpen);
    savePlaylistAction_ = make(tr("保存播放列表…"), "savePlaylistAction", {});
    setAppearanceIcon(savePlaylistAction_, AppearanceIcons::Symbol::Save);
    connect(savePlaylistAction_, &QAction::triggered, this, &MainWindow::choosePlaylistToSave);
    appendAction_ =
        make(tr("添加文件…"), "appendAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    setAppearanceIcon(appendAction_, AppearanceIcons::Symbol::Add);
    connect(appendAction_, &QAction::triggered, this, &MainWindow::chooseFilesToAppend);
    auto *mediaLibraryAction =
        make(tr("媒体库…"), "mediaLibraryAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L));
    setAppearanceIcon(mediaLibraryAction, AppearanceIcons::Symbol::MediaFile);
    connect(mediaLibraryAction, &QAction::triggered, this, [this] {
        LibraryDialogSupport::show(
            this, [this](const QString &path) { openFile(path); },
            [this](const QStringList &paths) { appendFiles(paths); });
    });
    pauseAction_ = make(tr("暂停"), "pauseAction", QKeySequence(Qt::Key_Space));
    connect(pauseAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playOrPause);
    connect(pauseAction_, &QAction::triggered, this, [this] {
        showActionFeedback(ActionFeedbackText::playback(pauseAction_->text() == tr("暂停")));
    });
    stopAction_ = make(tr("停止"), "stopAction", QKeySequence(Qt::Key_S));
    setAppearanceIcon(stopAction_, AppearanceIcons::Symbol::Stop);
    connect(stopAction_, &QAction::triggered, controller_.get(), &MpvController::stop);
    connect(stopAction_, &QAction::triggered, this, [this] { showActionFeedback(tr("已停止")); });
    previousAction_ = make(tr("上一个"), "previousAction", QKeySequence(Qt::Key_MediaPrevious));
    setAppearanceIcon(previousAction_, AppearanceIcons::Symbol::Previous);
    connect(previousAction_, &QAction::triggered, playlist_.get(),
            &PlaylistController::playPrevious);
    connect(previousAction_, &QAction::triggered, this,
            [this] { showActionFeedback(tr("上一个")); });
    nextAction_ = make(tr("下一个"), "nextAction", QKeySequence(Qt::Key_MediaNext));
    setAppearanceIcon(nextAction_, AppearanceIcons::Symbol::Next);
    connect(nextAction_, &QAction::triggered, playlist_.get(), &PlaylistController::playNext);
    connect(nextAction_, &QAction::triggered, this, [this] { showActionFeedback(tr("下一个")); });
    clearPlaylistAction_ = make(tr("清空播放列表"), "clearPlaylistAction", {});
    setAppearanceIcon(clearPlaylistAction_, AppearanceIcons::Symbol::Remove);
    connect(clearPlaylistAction_, &QAction::triggered, this, &MainWindow::clearPlaylist);
    muteAction_ = make(tr("静音"), "muteAction", QKeySequence(Qt::Key_M));
    muteAction_->setCheckable(true);
    connect(muteAction_, &QAction::triggered, controller_.get(), &MpvController::setMuted);
    connect(muteAction_, &QAction::triggered, this,
            [this](bool muted) { showActionFeedback(ActionFeedbackText::mute(muted)); });
    fullscreenAction_ = make(tr("全屏"), "fullscreenAction", QKeySequence(Qt::Key_F));
    fullscreenAction_->setCheckable(true);
    setAppearanceIcon(fullscreenAction_, AppearanceIcons::Symbol::Fullscreen);
    connect(fullscreenAction_, &QAction::triggered, this, &MainWindow::setFullscreen);
    exitFullscreenAction_ =
        make(tr("退出全屏"), "exitFullscreenAction", QKeySequence(Qt::Key_Escape));
    // Escape must not stay active outside fullscreen: it would be ambiguous
    // with the playlist pane's clear-search action and fire neither.
    exitFullscreenAction_->setEnabled(false);
    setAppearanceIcon(exitFullscreenAction_, AppearanceIcons::Symbol::Restore);
    connect(exitFullscreenAction_, &QAction::triggered, this, [this] { setFullscreen(false); });
    seekBackAction_ = make(tr("后退 5 秒"), "seekBackAction", QKeySequence(Qt::Key_Left));
    seekForwardAction_ = make(tr("前进 5 秒"), "seekForwardAction", QKeySequence(Qt::Key_Right));
    setAppearanceIcon(seekBackAction_, AppearanceIcons::Symbol::SeekBackward);
    setAppearanceIcon(seekForwardAction_, AppearanceIcons::Symbol::SeekForward);
    connect(seekBackAction_, &QAction::triggered, this, [this] {
        const auto seconds = seekStepSeconds();
        controller_->seekRelative(-seconds);
        showSeekFeedback(ActionFeedbackText::seek(-seconds));
    });
    connect(seekForwardAction_, &QAction::triggered, this, [this] {
        const auto seconds = seekStepSeconds();
        controller_->seekRelative(seconds);
        showSeekFeedback(ActionFeedbackText::seek(seconds));
    });
    auto *louder = make(tr("增加音量"), "volumeUpAction", QKeySequence(Qt::Key_Up));
    auto *quieter = make(tr("降低音量"), "volumeDownAction", QKeySequence(Qt::Key_Down));
    setAppearanceIcon(louder, AppearanceIcons::Symbol::Add);
    setAppearanceIcon(quieter, AppearanceIcons::Symbol::Remove);
    connect(louder, &QAction::triggered, this, [this] {
        const auto step = static_cast<double>(interactionVolumeStep());
        const auto volume = qBound(0.0, controller_->state().volume + step, 100.0);
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    connect(quieter, &QAction::triggered, this, [this] {
        const auto step = static_cast<double>(interactionVolumeStep());
        const auto volume = qBound(0.0, controller_->state().volume - step, 100.0);
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    auto *quit = make(tr("退出"), "quitAction", QKeySequence::Quit);
    setAppearanceIcon(quit, AppearanceIcons::Symbol::Remove);
    connect(quit, &QAction::triggered, this, &QWidget::close);
    auto *information =
        make(tr("媒体信息…"), "mediaInformationAction", QKeySequence(Qt::CTRL | Qt::Key_F1));
    setAppearanceIcon(information, AppearanceIcons::Symbol::Information);
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
    // Sorting is a structural edit as well, even though it only changes layout.
    connect(model, &PlaylistModel::layoutAboutToBeChanged, folderMenu_, &FolderMenu::cancel);
    auto *recent = new RecentMediaMenu(*recentMedia_, this);
    connect(recent, &RecentMediaMenu::openRequested, this, &MainWindow::openFile);
    connect(recent, &RecentMediaMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message); });
    connect(recent, &RecentMediaMenu::cleared, this, [this] {
        if (session_)
            session_->clearPlaybackHistory();
    });
    auto *currentMedia = new CurrentMediaMenu(*controller_, this);
    connect(currentMedia, &CurrentMediaMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message); });
    connect(currentMedia, &CurrentMediaMenu::errorOccurred, this, &MainWindow::showError);
    auto *file = menuBar()->addMenu(tr("文件(&F)"));
    file->addAction(openAction_);
    file->addAction(openNetworkAction_);
    file->addAction(appendAction_);
    file->addMenu(folderMenu_->menu());
    file->addAction(mediaLibraryAction);
    file->addMenu(recent->menu());
    file->addMenu(currentMedia->menu());
    file->addSeparator();
    file->addActions({openPlaylistAction_, savePlaylistAction_});
    file->addSeparator();
    file->addAction(information);
    file->addSeparator();
    file->addAction(quit);
    auto *playback = menuBar()->addMenu(tr("播放(&P)"));
    playback->addActions({previousAction_, pauseAction_, nextAction_, stopAction_});
    playback->addSeparator();
    playback->addActions({seekBackAction_, seekForwardAction_});
    auto *navigation = new NavigationMenu(*controller_, this);
    connect(navigation, &NavigationMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 4000); });
    connect(navigation, &NavigationMenu::seekFeedback, this, &MainWindow::showSeekFeedback);
    playback->addMenu(navigation->menu());
    auto *tuning = new TuningMenus(*controller_, this);
    connect(tuning, &TuningMenus::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    playback->addMenu(tuning->speedMenu());
    auto *audio = menuBar()->addMenu(tr("声音(&A)"));
    audio->addActions({muteAction_, louder, quieter});
    auto *subtitleDisplay =
        new SubtitleDisplayMenu(*controller_, this, [this](PreferencesDialog::Category category) {
            if (preferencesCoordinator_)
                preferencesCoordinator_->show(this, category);
        });
    connect(subtitleDisplay, &SubtitleDisplayMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    trackMenus_ =
        new TrackMenus(*controller_, this, tuning->subtitleDelayMenu(), subtitleDisplay->menu());
    auto *audioDevices = new AudioDeviceMenu(*controller_, this);
    audio->addSeparator();
    audio->addMenu(audioDevices->menu());
    audio->addMenu(trackMenus_->audioMenu());
    audio->addMenu(tuning->audioDelayMenu());
    audio->addMenu(tuning->equalizerMenu());
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
    auto *miniPlayer =
        make(tr("小窗播放"), "miniPlayerAction", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    miniPlayer->setCheckable(true);
    setAppearanceIcon(miniPlayer, AppearanceIcons::Symbol::Restore);
    connect(miniPlayer, &QAction::toggled, this, &MainWindow::setMiniPlayer);
    view->addAction(miniPlayer);
    alwaysOnTopAction_ = make(tr("窗口置顶"), "alwaysOnTopAction", {});
    alwaysOnTopAction_->setCheckable(true);
    connect(alwaysOnTopAction_, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);
    view->addAction(alwaysOnTopAction_);
    auto *playbackInformation = make(tr("显示当前播放信息"), "playbackInformationAction",
                                     QKeySequence(Qt::CTRL | Qt::Key_Tab));
    setAppearanceIcon(playbackInformation, AppearanceIcons::Symbol::Information);
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
    setAppearanceIcon(showPlaylistAction_, AppearanceIcons::Symbol::Playlist);
    connect(showPlaylistAction_, &QAction::toggled, this, [this](bool visible) {
        playlistPane_->setVisible(visible);
        if (visible)
            applyPlaylistWidth();
    });
    view->addAction(showPlaylistAction_);
    auto *settings = menuBar()->addMenu(tr("设置(&S)"));
    QSettings appearanceSettings;
    auto *appearanceMenu = new AppearanceMenu(this, appearanceSettings,
                                              qApp->property("configurationEnabled").toBool());
    preferencesCoordinator_ = new PreferencesCoordinator(
        *controller_,
        {.appearance = [appearanceMenu] { return appearanceMenu->settings(); },
         .setAppearance =
             [appearanceMenu](AppearancePreferences::Settings appearance) {
                 appearanceMenu->setSettings(appearance);
             },
         .mediaLibrary = [this] { return folderMenu_->mediaLibraryPreferences(); },
         .setMediaLibrary =
             [this](MediaLibraryPreferences preferences) {
                 folderMenu_->setMediaLibraryPreferences(preferences);
                 if (session_)
                     session_->setMediaLibraryPreferences(preferences);
             },
         .resumePlaybackPosition =
             [this] {
                 return session_ ? session_->resumePlaybackPositionEnabled()
                                 : resumePlaybackPositionDefault_;
             },
         .setResumePlaybackPosition =
             [this](bool enabled) {
                 resumePlaybackPositionDefault_ = enabled;
                 if (session_)
                     session_->setResumePlaybackPositionEnabled(enabled);
             },
         .seekStepSeconds = [this] { return seekStepSeconds(); },
         .setSeekStepSeconds =
             [this](int seconds) {
                 seekStepSecondsDefault_ = seconds;
                 if (session_)
                     session_->setSeekStepSeconds(seconds);
                 updateSeekActionLabels(seconds);
             },
         .interaction =
             [this] {
                 return session_ ? session_->interactionPreferences().normalized()
                                 : interactionPreferences_.normalized();
             },
         .setInteraction =
             [this](InteractionPreferences preferences) {
                 applyInteractionPreferences(preferences);
                 if (session_)
                     session_->setInteractionPreferences(interactionPreferences_);
             },
         .language =
             [this] {
                 return session_ ? session_->languagePreferences().normalized()
                                 : languagePreferences_;
             },
         .setLanguage =
             [this](LanguagePreferences preferences) {
                 languagePreferences_ = preferences.normalized();
                 if (session_)
                     session_->setLanguagePreferences(languagePreferences_);
             }},
        this);
    const auto showPreferences = [this](PreferencesDialog::Category category) {
        preferencesCoordinator_->show(this, category);
    };
    auto *playbackSettings =
        make(tr("播放设置…"), "playbackSettingsAction", QKeySequence(Qt::Key_F5));
    setAppearanceIcon(playbackSettings, AppearanceIcons::Symbol::Settings);
    connect(playbackSettings, &QAction::triggered, this,
            [showPreferences] { showPreferences(PreferencesDialog::Category::Playback); });
    settings->addAction(playbackSettings);
    auto *mediaLibrarySettings = settings->addAction(tr("媒体库设置…"));
    mediaLibrarySettings->setObjectName(QStringLiteral("mediaLibrarySettingsAction"));
    setAppearanceIcon(mediaLibrarySettings, AppearanceIcons::Symbol::Settings);
    connect(mediaLibrarySettings, &QAction::triggered, this,
            [showPreferences] { showPreferences(PreferencesDialog::Category::MediaLibrary); });
    settings->addAction(subtitleDisplay->settingsAction());
    auto *appearanceSettingsAction = settings->addAction(tr("外观设置…"));
    appearanceSettingsAction->setObjectName(QStringLiteral("appearanceSettingsAction"));
    setAppearanceIcon(appearanceSettingsAction, AppearanceIcons::Symbol::Settings);
    connect(appearanceSettingsAction, &QAction::triggered, this,
            [showPreferences] { showPreferences(PreferencesDialog::Category::Appearance); });
    auto *interactionSettings = settings->addAction(tr("交互设置…"));
    interactionSettings->setObjectName(QStringLiteral("interactionSettingsAction"));
    setAppearanceIcon(interactionSettings, AppearanceIcons::Symbol::Settings);
    connect(interactionSettings, &QAction::triggered, this,
            [showPreferences] { showPreferences(PreferencesDialog::Category::Interaction); });
    settings->addSeparator();
    auto *themeMenu = appearanceMenu->menu();
    themeMenu->setTitle(tr("主题"));
    settings->addMenu(themeMenu);
    themeMenu->menuAction()->setObjectName(QStringLiteral("themeMenuAction"));
    auto *languageMenu = settings->addMenu(tr("语言"));
    languageMenu->menuAction()->setObjectName(QStringLiteral("languageMenuAction"));
    auto *languageGroup = new QActionGroup(languageMenu);
    languageGroup->setExclusive(true);
    const auto savedLanguage =
        qApp->property("configurationEnabled").toBool()
            ? appearanceSettings
                  .value(QStringLiteral("appearance/language"), QStringLiteral("zh_CN"))
                  .toString()
            : QStringLiteral("zh_CN");
    for (const auto &[name, key] : {std::pair{tr("简体中文"), QStringLiteral("zh_CN")},
                                    std::pair{QStringLiteral("English"), QStringLiteral("en")}}) {
        auto *action = languageMenu->addAction(name);
        action->setObjectName(key == QLatin1String("en") ? QStringLiteral("languageEnglishAction")
                                                         : QStringLiteral("languageChineseAction"));
        action->setCheckable(true);
        action->setEnabled(qApp->property("configurationEnabled").toBool());
        languageGroup->addAction(action);
        action->setChecked(savedLanguage == key);
        connect(action, &QAction::triggered, this, [this, key] {
            auto *manager = static_cast<LanguageManager *>(
                qApp->property("languageManager").value<QObject *>());
            if (!manager || !manager->setLanguage(key))
                return;
            if (property("configurationEnabled").toBool()) {
                QSettings preferences;
                preferences.setValue(QStringLiteral("appearance/language"), key);
            }
        });
    }
    auto *shortcutsAction = make(tr("快捷键…"), "shortcutsAction", QKeySequence(Qt::Key_F1));
    connect(shortcutsAction, &QAction::triggered, this, [this] {
        const auto extraActions =
            playlistPane_ ? playlistPane_->shortcutActions() : QList<QAction *>{};
        ShortcutsDialog dialog(menuBar(), extraActions, this);
        dialog.exec();
    });
    auto *about = settings->addAction(tr("关于 NekoPlayer…"));
    about->setObjectName(QStringLiteral("aboutAction"));
    setAppearanceIcon(about, AppearanceIcons::Symbol::Information);
    about->setToolTip(about->text());
    about->setStatusTip(about->text());
    connect(about, &QAction::triggered, this,
            [this] { AppearancePreferences::showAboutDialog(this); });
    auto *help = menuBar()->addMenu(tr("帮助(&H)"));
    helpMenu_ = help;
    help->addAction(shortcutsAction);
    help->addSeparator();
    help->addAction(about);
    contextMenu_ = new QMenu(this);
    contextMenu_->addActions(
        {openAction_, openNetworkAction_, appendAction_, pauseAction_, stopAction_});
    contextMenu_->addMenu(folderMenu_->menu());
    contextMenu_->addMenu(recent->menu());
    contextMenu_->addMenu(currentMedia->menu());
    contextMenu_->addSeparator();
    contextMenu_->addMenu(navigation->menu());
    contextMenu_->addMenu(tuning->speedMenu());
    contextMenu_->addMenu(tuning->audioDelayMenu());
    contextMenu_->addMenu(tuning->equalizerMenu());
    contextMenu_->addMenu(modeMenu->menu());
    contextMenu_->addActions({previousAction_, nextAction_, showPlaylistAction_, muteAction_,
                              fullscreenAction_, miniPlayer});
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

void MainWindow::updateSeekActionLabels(int seconds) {
    for (const auto &[name, direction] :
         {std::pair{"seekBackAction", -1}, std::pair{"seekForwardAction", 1}}) {
        if (auto *action = findChild<QAction *>(QString::fromLatin1(name))) {
            const auto label =
                direction < 0 ? tr("后退 %1 秒").arg(seconds) : tr("前进 %1 秒").arg(seconds);
            action->setText(label);
            action->setToolTip(label);
            action->setStatusTip(label);
        }
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::PaletteChange)
        refreshAppearanceIcons();
    if (event->type() == QEvent::WindowStateChange && !syncingWindowState_) {
        const auto fullscreen = isFullScreen();
        if (fullscreen != fullscreenPresentationActive_)
            setFullscreen(fullscreen);
    }
    if (event->type() != QEvent::LanguageChange)
        return;

    const auto setActionText = [this](const char *name, const QString &text) {
        if (auto *action = findChild<QAction *>(QString::fromLatin1(name))) {
            action->setText(text);
            action->setToolTip(text);
            action->setStatusTip(text);
        }
    };
    setActionText("openAction", tr("打开文件…"));
    setActionText("openNetworkAction", tr("打开网络地址…"));
    setActionText("openPlaylistAction", tr("打开播放列表…"));
    setActionText("savePlaylistAction", tr("保存播放列表…"));
    setActionText("appendAction", tr("添加文件…"));
    setActionText("mediaLibraryAction", tr("媒体库…"));
    setActionText("pauseAction", controller_->state().paused ? tr("播放") : tr("暂停"));
    setActionText("stopAction", tr("停止"));
    setActionText("previousAction", tr("上一个"));
    setActionText("nextAction", tr("下一个"));
    setActionText("clearPlaylistAction", tr("清空播放列表"));
    if (playlistMaintenance_)
        playlistMaintenance_->retranslate();
    setActionText("muteAction", tr("静音"));
    setActionText("fullscreenAction", isFullScreen() ? tr("退出全屏") : tr("全屏"));
    setActionText("miniPlayerAction", miniPlayerActive_ ? tr("退出小窗") : tr("小窗播放"));
    setActionText("alwaysOnTopAction", tr("窗口置顶"));
    const auto menus = menuBar()->actions();
    const QStringList titles{tr("文件(&F)"), tr("播放(&P)"), tr("声音(&A)"),
                             tr("字幕(&U)"), tr("视频(&I)"), tr("播放列表(&L)"),
                             tr("视图(&V)"), tr("设置(&S)"), tr("帮助(&H)")};
    for (int index = 0; index < qMin(menus.size(), titles.size()); ++index)
        menus.at(index)->setText(titles.at(index));
    if (auto *theme = findChild<QAction *>(QStringLiteral("themeMenuAction")))
        theme->setText(tr("主题"));
    if (auto *language = findChild<QAction *>(QStringLiteral("languageMenuAction")))
        language->setText(tr("语言"));
    if (auto *english = findChild<QAction *>(QStringLiteral("languageEnglishAction")))
        english->setText(QStringLiteral("English"));
    if (auto *chinese = findChild<QAction *>(QStringLiteral("languageChineseAction")))
        chinese->setText(tr("简体中文"));
    setActionText("playbackSettingsAction", tr("播放设置…"));
    setActionText("mediaLibrarySettingsAction", tr("媒体库设置…"));
    setActionText("appearanceSettingsAction", tr("外观设置…"));
    setActionText("interactionSettingsAction", tr("交互设置…"));
    setActionText("aboutAction", tr("关于 NekoPlayer…"));
    setActionText("shortcutsAction", tr("快捷键…"));
    setActionText("themeDarkAction",
                  AppearancePreferences::themeName(AppearancePreferences::Theme::Dark));
    setActionText("themeLightAction",
                  AppearancePreferences::themeName(AppearancePreferences::Theme::Light));
    setActionText("themeSystemAction",
                  AppearancePreferences::themeName(AppearancePreferences::Theme::System));
    for (auto *menu : findChildren<SubtitleDisplayMenu *>())
        menu->retranslate();
    if (auto *video = findChild<VideoMenu *>())
        video->retranslate();
    for (auto *menu : findChildren<NavigationMenu *>())
        menu->retranslate();
    for (auto *menu : findChildren<TuningMenus *>())
        menu->retranslate();
    for (auto *menu : findChildren<AbLoopMenu *>())
        menu->retranslate();
    for (auto *menu : findChildren<PlaylistModeMenu *>())
        menu->retranslate();
}
