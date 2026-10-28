#include "mainwindow.h"
#include "actionfeedbackoverlay.h"
#include "actionfeedbacktext.h"
#include "appearanceicons.h"
#include "foldermenu.h"
#include "fullscreencontrols.h"
#include "input/localfiledrop.h"
#include "input/mediafiletypes.h"
#include "interactionpolicy.h"
#include "networkopendialog.h"
#include "playback/mediasource.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "playbackoverlay.h"
#include "playlist/m3u8store.h"
#include "playlistmaintenancemenu.h"
#include "playlistpane.h"
#include "preview/thumbnailservice.h"
#include "quickactionsdialog.h"
#include "session/recentmediahistory.h"
#include "session/sessionmanager.h"
#include "shortcutsdialog.h"
#include "thumbnailpreview.h"
#include "thumbnailpreviewpolicy.h"
#include "transportbar.h"
#include "welcomepanel.h"
#include "windowopacitytransition.h"
#include "windowtitle.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>
#include <algorithm>
#include <utility>

namespace {
QStringList normalizedSources(const QStringList &sources) {
    QStringList result;
    result.reserve(sources.size());
    for (const auto &source : sources) {
        const auto normalized = MediaSource::normalize(source);
        if (normalized.isEmpty())
            return {};
        result.append(normalized);
    }
    return result;
}
} // namespace

MainWindow::MainWindow(bool silentAudio, const QString &sessionPath)
    : controller_(std::make_unique<MpvController>(false, silentAudio)),
      playlist_(std::make_unique<PlaylistController>(*controller_)),
      recentMedia_(std::make_unique<RecentMediaHistory>()) {
    setWindowTitle(QStringLiteral("NekoPlayer"));
    // Keep the toplevel alpha-capable; the QSS switches between transparent
    // glass chrome and the opaque canvas when glass is disabled.
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(1100, 680);
    setMinimumSize(560, 360);
    setAcceptDrops(true);
    fileDrop_ = new LocalFileDrop(*controller_, this);
    connect(fileDrop_, &LocalFileDrop::mediaFilesDropped, this, &MainWindow::appendFiles);
    connect(fileDrop_, &LocalFileDrop::errorOccurred, this, &MainWindow::showError);
    createActions();
    new QuickActionsDialog(this, this);
    if (auto *quickActions = findChild<QAction *>(QStringLiteral("quickActionsAction")))
        helpMenu_->addAction(quickActions);
    connect(folderMenu_, &FolderMenu::mediaLibraryPreferencesChanged, this,
            [this](bool scanningEnabled, bool recursiveByDefault) {
                if (session_)
                    session_->setMediaLibraryPreferences({scanningEnabled, recursiveByDefault});
            });
    connect(fileDrop_, &LocalFileDrop::foldersDropped, this, [this](const QStringList &paths) {
        folderMenu_->start(paths, true, folderMenu_->recursiveEnabled());
    });
    auto *content = new QWidget(this);
    content->setAttribute(Qt::WA_TranslucentBackground, true);
    auto *layout = new QVBoxLayout(content);
    centralLayout_ = layout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    video_ = new VideoWidget(*controller_, content);
    // The idle video area shares the window's alpha surface so the whole
    // non-video chrome can show the compositor-blurred backdrop.
    video_->setAttribute(Qt::WA_TranslucentBackground, true);
    feedbackOverlay_ = new ActionFeedbackOverlay(video_);
    feedbackOverlay_->setGeometry(video_->rect());
    playbackOverlay_ = new PlaybackOverlay(*controller_, video_);
    playbackOverlay_->setGeometry(video_->rect());
    welcomePanel_ = new WelcomePanel(openAction_, openNetworkAction_, video_);
    connect(welcomePanel_, &WelcomePanel::recentSourceRequested, this, &MainWindow::openFile);
    connect(welcomePanel_, &WelcomePanel::resumeSourceRequested, this, &MainWindow::openFile);
    connect(recentMedia_.get(), &RecentMediaHistory::changed, this,
            [this] { welcomePanel_->setRecentSources(recentMedia_->entries()); });
    welcomePanel_->setRecentSources(recentMedia_->entries());
    auto *videoLayout = new QVBoxLayout(video_);
    videoLayout->setContentsMargins(24, 24, 24, 24);
    videoLayout->setSpacing(0);
    videoLayout->addStretch(1);
    videoLayout->addWidget(welcomePanel_, 0, Qt::AlignHCenter);
    videoLayout->addStretch(1);
    playlistPane_ = new PlaylistPane(*playlist_, appendAction_, previousAction_, nextAction_,
                                     clearPlaylistAction_, content);
    playlistMenu_->addSeparator();
    playlistMenu_->addActions(playlistPane_->editingActions());
    playlistMaintenance_ = new PlaylistMaintenanceMenu(*playlist_, this, this);
    connect(playlistMaintenance_, &PlaylistMaintenanceMenu::feedback, this,
            [this](const QString &message) { showActionFeedback(message, 2500); });
    playlistMenu_->addSeparator();
    playlistMenu_->addMenu(playlistMaintenance_->sortMenu());
    playlistMenu_->addAction(playlistMaintenance_->removeMissingAction());
    playlistPane_->hide();
    splitter_ = new QSplitter(Qt::Horizontal, content);
    splitter_->setObjectName(QStringLiteral("bodySplitter"));
    splitter_->addWidget(video_);
    splitter_->addWidget(playlistPane_);
    splitter_->setStretchFactor(0, 1);
    splitter_->setStretchFactor(1, 0);
    splitter_->setCollapsible(0, false);
    splitter_->setCollapsible(1, false);
    connect(splitter_, &QSplitter::splitterMoved, this, [this] { rememberPlaylistWidth(); });
    transport_ = new TransportBar(openAction_, previousAction_, pauseAction_, nextAction_,
                                  stopAction_, muteAction_, fullscreenAction_, content);
    thumbnailService_ = new ThumbnailService(this);
    thumbnailPreview_ = new ThumbnailPreview(this);
    fullscreenControls_ = new FullscreenControls(*transport_, *video_, *centralLayout_, this);
    layout->addWidget(splitter_, 1);
    layout->addWidget(transport_);
    setCentralWidget(content);
    miniPlayerAnimation_ = new WindowOpacityTransition(*this, this);
    connect(miniPlayerAnimation_, &WindowOpacityTransition::reached, this, [this](qreal opacity) {
        if (opacity == 0.0) {
            applyMiniPlayerState(miniPlayerDesired_);
            animateMiniPlayerWindow(1.0);
        } else if (miniPlayerDesired_ != miniPlayerActive_) {
            animateMiniPlayerWindow(0.0);
        }
    });
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setAttribute(Qt::WA_TranslucentBackground, true);
    statusBar()->setAttribute(Qt::WA_NoSystemBackground, true);
    statusBar()->setAutoFillBackground(false);
    statusBar()->setObjectName(QStringLiteral("statusBar"));
    statusBar()->showMessage(tr("就绪 · Qt 6 + libmpv"));
    connect(controller_.get(), &MpvController::stateChanged, this, &MainWindow::updateState);
    connect(controller_.get(), &MpvController::errorOccurred, this, &MainWindow::showError);
    connect(controller_.get(), &MpvController::fileOpenRequested, this,
            &MainWindow::hideThumbnailPreview);
    connect(controller_.get(), &MpvController::stopRequested, this,
            &MainWindow::hideThumbnailPreview);
    connect(controller_.get(), &MpvController::mediaLoadFailed, this,
            &MainWindow::hideThumbnailPreview);
    connect(controller_.get(), &MpvController::playbackFinished, this,
            &MainWindow::hideThumbnailPreview);
    connect(controller_.get(), &MpvController::fileLoaded, this, [this] {
        recentMedia_->remember(controller_->state().path);
        statusBar()->showMessage(
            tr("正在播放 · %1").arg(MediaSource::displayName(controller_->state().path)));
    });
    // --start-paused pauses once on the first successful load. Failed loads and
    // explicit stops consume the pending request so it cannot affect a later
    // user-initiated load.
    connect(controller_.get(), &MpvController::fileLoaded, this, [this] {
        if (std::exchange(startPaused_, false)) {
            controller_->setPaused(true);
            statusBar()->showMessage(
                tr("已暂停 · %1").arg(MediaSource::displayName(controller_->state().path)));
        }
    });
    connect(controller_.get(), &MpvController::mediaLoadFailed, this,
            [this] { startPaused_ = false; });
    connect(controller_.get(), &MpvController::stopRequested, this,
            [this] { startPaused_ = false; });
    connect(playlist_.get(), &PlaylistController::queueFinished, this,
            [this] { statusBar()->showMessage(tr("播放列表已结束")); });
    connect(playlist_.get(), &PlaylistController::queueError, this, &MainWindow::showError);
    connect(playlist_.get(), &PlaylistController::currentChanged, this, &MainWindow::updateState);
    connect(playlist_.get(), &PlaylistController::repeatModeChanged, this,
            &MainWindow::updateState);
    connect(playlist_.get(), &PlaylistController::shuffleEnabledChanged, this,
            &MainWindow::updateState);
    connect(playlist_.get(), &PlaylistController::navigationChanged, this,
            &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::rowsInserted, this, &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::rowsRemoved, this, &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::rowsMoved, this, &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::modelReset, this, &MainWindow::updateState);
    connect(playlistPane_, &PlaylistPane::playRequested, playlist_.get(), [this](int row) {
        folderMenu_->cancel();
        if (video_->isReady())
            playlist_->playAt(row);
    });
    connect(video_, &VideoWidget::renderError, this, [this](const QString &message) {
        showError(message);
        updateState();
    });
    connect(video_, &VideoWidget::contextResetting, controller_.get(),
            &MpvController::prepareForRendererReset);
    connect(video_, &VideoWidget::singleClicked, this, [this] {
        if (interactionPreferences_.singleClickPlayPause)
            pauseAction_->trigger();
    });
    connect(video_, &VideoWidget::doubleClicked, this, [this] {
        if (interactionPreferences_.doubleClickFullscreen)
            fullscreenAction_->trigger();
    });
    connect(video_, &VideoWidget::middleClicked, this, [this] {
        switch (interactionPreferences_.middleClick) {
        case MiddleClickAction::None:
            break;
        case MiddleClickAction::PlayPause:
            pauseAction_->trigger();
            break;
        case MiddleClickAction::Mute:
            muteAction_->trigger();
            break;
        case MiddleClickAction::Fullscreen:
            fullscreenAction_->trigger();
            break;
        }
    });
    connect(video_, &VideoWidget::wheelScrolled, this, [this](int deltaY) {
        if (deltaY == 0 || !interactionPreferences_.wheelAdjustsVolume || !controller_->isReady() ||
            !controller_->state().active)
            return;
        const auto volume =
            volumeAfterWheel(controller_->state().volume, deltaY, interactionVolumeStep());
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    connect(video_, &VideoWidget::ready, this, [this] {
        const auto pendingPlaylistIndex =
            std::exchange(pendingPlaylistIndex_, QPersistentModelIndex{});
        if (pendingPlaylistIndex.isValid())
            playlist_->playAt(pendingPlaylistIndex.row());
        if (!pendingPaths_.isEmpty()) {
            const auto paths = std::exchange(pendingPaths_, {});
            playlist_->addFiles(paths);
        }
        // Appending files must not strand the recovery snapshot: only a new
        // explicit playback request supersedes the file we were recovering.
        if (!pendingPlaylistIndex.isValid())
            controller_->resumeAfterRendererReset();
        updateState();
    });
    connect(transport_, &TransportBar::seekRequested, this, [this](double seconds) {
        controller_->seekAbsolute(seconds);
        showSeekFeedback(ActionFeedbackText::seekTo(seconds));
    });
    connect(transport_, &TransportBar::volumeRequested, this, [this](double volume) {
        controller_->setVolume(volume);
        showActionFeedback(ActionFeedbackText::volume(qRound(volume)));
    });
    connect(transport_, &TransportBar::wheelSeekRequested, this, [this](int direction) {
        if (!interactionPreferences_.wheelSeeksTimeline)
            return;
        const auto seconds = seekStepSeconds();
        controller_->seekRelative(direction * seconds);
        showSeekFeedback(ActionFeedbackText::seek(direction * seconds));
    });
    connect(transport_, &TransportBar::timelineHovered, this, &MainWindow::requestThumbnailPreview);
    connect(transport_, &TransportBar::timelineHoverEnded, this, &MainWindow::hideThumbnailPreview);
    connect(thumbnailService_, &ThumbnailService::thumbnailReady, this,
            &MainWindow::showThumbnailPreview);
    controlsTimer_.setSingleShot(true);
    controlsTimer_.setInterval(2200);
    connect(&controlsTimer_, &QTimer::timeout, this, [this] {
        if (isFullScreen() && !fullscreenControls_->interactionActive() &&
            !contextMenu_->isVisible()) {
            fullscreenControls_->fadeOut();
            statusBar()->hide();
            video_->setCursor(Qt::BlankCursor);
        } else if (isFullScreen())
            controlsTimer_.start();
    });
    installEventFilter(this);
    const auto children = findChildren<QWidget *>();
    for (auto *child : children) {
        child->setMouseTracking(true);
        child->installEventFilter(this);
    }
    // Created after the menus and panes so every action with a default
    // shortcut is captured; windowactions.cpp discovers it via qApp.
    shortcutSettings_ = new ShortcutSettings(this, {}, this);
    qApp->setProperty("shortcutSettings",
                      QVariant::fromValue(static_cast<QObject *>(shortcutSettings_)));
    restoreSession(sessionPath);
    updateState();
    if (!controller_->isReady())
        showError(controller_->errorString());
}

MainWindow::~MainWindow() {
    if (qApp && qApp->property("shortcutSettings").value<QObject *>() == shortcutSettings_)
        qApp->setProperty("shortcutSettings", QVariant{});
    folderMenu_->cancel();
    controlsTimer_.stop();
    if (!sessionSaveAttempted_) {
        if (const auto error = saveSession(); !error.isEmpty())
            qWarning().noquote() << error;
    }
    // QMainWindow deletes children after members. GPU resources must go first.
    delete fullscreenControls_;
    delete takeCentralWidget();
}
MpvController *MainWindow::controller() const { return controller_.get(); }
VideoWidget *MainWindow::videoWidget() const { return video_; }
PlaylistController *MainWindow::playlist() const { return playlist_.get(); }

void MainWindow::setStartPaused(bool paused) { startPaused_ = paused; }

void MainWindow::enterFullscreen() { setFullscreen(true); }

void MainWindow::openFile(const QString &path) { openFiles({path}); }

void MainWindow::openFiles(const QStringList &paths) {
    folderMenu_->cancel();
    // A rejected open must not leave the --start-paused one-shot armed; only an
    // accepted startup open may carry it to fileLoaded.
    if (paths.isEmpty()) {
        startPaused_ = false;
        return;
    }
    const auto sources = normalizedSources(paths);
    if (sources.isEmpty()) {
        startPaused_ = false;
        showError(tr("媒体地址无效；网络地址需要包含受支持的协议与主机名"));
        return;
    }
    if (video_->hasFailed()) {
        startPaused_ = false;
        showError(tr("OpenGL 初始化失败，无法打开媒体文件"));
        return;
    }
    clearPlaylist();
    appendFiles(sources);
}

void MainWindow::appendFiles(const QStringList &paths) {
    folderMenu_->cancel();
    if (paths.isEmpty())
        return;
    const auto sources = normalizedSources(paths);
    if (sources.isEmpty()) {
        showError(tr("媒体地址无效；未添加到播放列表"));
        return;
    }
    if (session_)
        session_->rememberMedia(sources.constFirst());
    if (video_->isReady())
        playlist_->addFiles(sources);
    else
        pendingPaths_.append(sources);
    updateState();
}

void MainWindow::clearPlaylist() {
    folderMenu_->cancel();
    pendingPlaylistIndex_ = QPersistentModelIndex{};
    pendingPaths_.clear();
    playlist_->clear();
}

void MainWindow::openPlaylist(const QString &path) {
    folderMenu_->cancel();
    if (video_->hasFailed()) {
        showError(tr("OpenGL 初始化失败，无法打开播放列表"));
        return;
    }
    const auto loaded = M3u8Store::load(path);
    if (!loaded.error.isEmpty()) {
        showError(loaded.error);
        return;
    }
    const bool containsNetworkSource =
        std::any_of(loaded.paths.cbegin(), loaded.paths.cend(),
                    [](const QString &source) { return MediaSource::isNetwork(source); });
    pendingPaths_.clear();
    pendingPlaylistIndex_ = QPersistentModelIndex{};
    QList<PlaylistEntryMetadata> metadata;
    metadata.reserve(loaded.entries.size());
    for (const auto &entry : loaded.entries)
        metadata.append({entry.durationSeconds, entry.title});
    playlist_->restoreFilesWithMetadata(loaded.paths, loaded.currentIndex, 0, metadata);
    if (containsNetworkSource) {
        // An imported queue can name arbitrary hosts. Select the saved row but
        // require an explicit Play/Space before resolving a network address.
        updateState();
        showActionFeedback(tr("已导入含网络地址的播放列表；确认后按空格开始播放"));
        return;
    }
    if (video_->isReady() && loaded.currentIndex >= 0)
        playlist_->playAt(loaded.currentIndex);
    else if (loaded.currentIndex >= 0)
        pendingPlaylistIndex_ = playlist_->model()->index(loaded.currentIndex);
    updateState();
}

void MainWindow::savePlaylist(const QString &path) {
    const auto &model = *playlist_->model();
    QList<M3u8Entry> entries;
    entries.reserve(model.rowCount());
    for (auto row = 0; row < model.rowCount(); ++row) {
        const auto metadata = model.metadataAt(row);
        entries.append({model.pathAt(row), metadata.durationSeconds, metadata.title});
    }
    const auto error = M3u8Store::saveEntries(path, entries, model.currentIndex());
    if (!error.isEmpty())
        showError(error);
    else
        showActionFeedback(tr("播放列表已保存"));
}

void MainWindow::chooseFile() {
    folderMenu_->cancel();
    const auto path = QFileDialog::getOpenFileName(
        this, tr("打开媒体文件"), mediaDirectory(),
        tr("媒体文件 (%1);;所有文件 (*)").arg(mediaFileNameFilterPatterns()));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::chooseNetworkUrl() {
    folderMenu_->cancel();
    NetworkOpenDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted)
        openFile(dialog.mediaUrl());
}

void MainWindow::chooseFilesToAppend() {
    folderMenu_->cancel();
    const auto paths = QFileDialog::getOpenFileNames(
        this, tr("添加媒体文件"), mediaDirectory(),
        tr("媒体文件 (%1);;所有文件 (*)").arg(mediaFileNameFilterPatterns()));
    appendFiles(paths);
}

void MainWindow::openFolder(const QString &directory, bool append, bool recursive) {
    if (video_->hasFailed()) {
        showError(tr("OpenGL 初始化失败，无法打开文件夹"));
        return;
    }
    folderMenu_->start(directory, append, recursive);
}

void MainWindow::updateState() {
    const auto &state = controller_->state();
    pauseAction_->setEnabled(video_->isReady() &&
                             (state.active || playlist_->model()->rowCount() > 0));
    stopAction_->setEnabled(state.active);
    const bool canSeek = state.active && state.seekable;
    seekBackAction_->setEnabled(canSeek);
    seekForwardAction_->setEnabled(canSeek);
    const bool canPlay = !state.active || state.paused;
    pauseAction_->setText(canPlay ? tr("播放") : tr("暂停"));
    pauseAction_->setToolTip(pauseAction_->text());
    pauseAction_->setStatusTip(pauseAction_->text());
    setAppearanceIcon(pauseAction_,
                      canPlay ? AppearanceIcons::Symbol::Play : AppearanceIcons::Symbol::Pause);
    muteAction_->setChecked(state.muted);
    setAppearanceIcon(muteAction_, state.muted ? AppearanceIcons::Symbol::Mute
                                               : AppearanceIcons::Symbol::Volume);
    previousAction_->setEnabled(video_->isReady() && playlist_->hasPrevious());
    nextAction_->setEnabled(video_->isReady() && playlist_->hasNext());
    clearPlaylistAction_->setEnabled(playlist_->model()->rowCount() > 0 ||
                                     !pendingPaths_.isEmpty());
    transport_->present(state);
    welcomePanel_->setVisible(!state.active);
    if (state.path != lastTitlePath_) {
        lastTitlePath_ = state.path;
        setWindowTitle(windowTitleForPath(lastTitlePath_));
    }
}

void MainWindow::showError(const QString &message) {
    feedbackOverlay_->clearMessage();
    const auto text = tr("无法播放：%1").arg(message);
    // The status bar is hidden in fullscreen and in the mini player; those modes
    // must still surface failures through the video overlay.
    if (isFullScreen() || miniPlayerActive_) {
        if (isFullScreen())
            revealControls();
        feedbackOverlay_->showMessage(text, 5000);
    } else {
        statusBar()->showMessage(text);
    }
    emit userError(message);
}

void MainWindow::showActionFeedback(const QString &message, int durationMs) {
    if (isFullScreen() || miniPlayerActive_)
        feedbackOverlay_->showMessage(message, durationMs);
    else
        statusBar()->showMessage(message, durationMs);
}

void MainWindow::showSeekFeedback(const QString &message) {
    if (playbackOverlay_->showSeek(message))
        feedbackOverlay_->clearMessage();
    else
        showActionFeedback(message);
}

void MainWindow::requestThumbnailPreview(double seconds, QPoint globalPosition) {
    const auto &state = controller_->state();
    if (!canRequestThumbnailPreview(state, controller_->canStepFrameForward())) {
        hideThumbnailPreview();
        return;
    }
    thumbnailAnchor_ = globalPosition;
    thumbnailPreview_->hide();
    const auto requestId = thumbnailService_->request(
        state.path, std::clamp(seconds, 0.0, state.duration), QSize(232, 130));
    Q_UNUSED(requestId)
}

void MainWindow::hideThumbnailPreview() {
    if (thumbnailService_)
        thumbnailService_->cancel();
    if (thumbnailPreview_)
        thumbnailPreview_->clear();
}

void MainWindow::showThumbnailPreview(quint64 requestId, const QString &source,
                                      double requestedSeconds, ThumbnailResult result) {
    Q_UNUSED(requestId)
    const auto &state = controller_->state();
    if (!state.active) {
        hideThumbnailPreview();
        return;
    }
    if (!result.succeeded() || source != state.path)
        return;
    thumbnailPreview_->present(result.image, requestedSeconds);
    const auto timeline = transport_->timelineGlobalGeometry();
    const auto size = thumbnailPreview_->sizeHint();
    const auto *screen = QGuiApplication::screenAt(timeline.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const auto available = screen->availableGeometry();
    const auto anchor = thumbnailAnchor_.isNull() ? timeline.center() : thumbnailAnchor_;
    const auto x = std::clamp(anchor.x() - size.width() / 2, available.left(),
                              available.right() - size.width() + 1);
    auto y = timeline.top() - size.height() - 10;
    if (y < available.top())
        y = timeline.bottom() + 10;
    thumbnailPreview_->move(x, y);
    thumbnailPreview_->show();
    thumbnailPreview_->raise();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    if (fileDrop_->handleUrls(event->mimeData()->urls()))
        event->acceptProposedAction();
    else
        event->ignore();
}

void MainWindow::setFullscreen(bool enabled) {
    if (syncingWindowState_)
        return;
    if (enabled == fullscreenPresentationActive_ && enabled == isFullScreen())
        return;
    exitFullscreenAction_->setEnabled(enabled);

    if (enabled && (miniPlayerActive_ || miniPlayerDesired_))
        setMiniPlayerImmediately(false);
    hideThumbnailPreview();

    syncingWindowState_ = true;
    if (enabled) {
        if (!fullscreenPresentationActive_) {
            const auto priorGeometry = normalGeometry();
            if (priorGeometry.isValid())
                normalGeometry_ = saveGeometry();
            normalMaximized_ = isMaximized();
            menuBar()->hide();
            statusBar()->hide();
            fullscreenControls_->enter();
            playlistPane_->hide();
            showPlaylistAction_->setEnabled(false);
            fullscreenPresentationActive_ = true;
        }
        if (!isFullScreen())
            showFullScreen();
        video_->setCursor(Qt::BlankCursor);
        revealControls();
    } else {
        controlsTimer_.stop();
        if (fullscreenPresentationActive_)
            fullscreenControls_->leave();
        fullscreenPresentationActive_ = false;
        video_->unsetCursor();
        if (isFullScreen()) {
            showNormal();
            if (!normalGeometry_.isEmpty())
                restoreGeometry(normalGeometry_);
            if (normalMaximized_)
                showMaximized();
        }
        if (miniPlayerActive_) {
            menuBar()->hide();
            statusBar()->hide();
            showPlaylistAction_->setEnabled(false);
            playlistPane_->hide();
        } else {
            menuBar()->show();
            statusBar()->show();
            showPlaylistAction_->setEnabled(true);
            playlistPane_->setVisible(showPlaylistAction_->isChecked());
            applyPlaylistWidth();
        }
    }
    if (!enabled && !miniPlayerActive_ &&
        windowFlags().testFlag(Qt::WindowStaysOnTopHint) != alwaysOnTop_) {
        const bool wasVisible = isVisible();
        setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop_);
        if (wasVisible)
            show();
    }
    fullscreenAction_->setChecked(enabled);
    fullscreenAction_->setText(enabled ? tr("退出全屏") : tr("全屏"));
    fullscreenAction_->setToolTip(fullscreenAction_->text());
    fullscreenAction_->setStatusTip(fullscreenAction_->text());
    setAppearanceIcon(fullscreenAction_, enabled ? AppearanceIcons::Symbol::Restore
                                                 : AppearanceIcons::Symbol::Fullscreen);
    syncingWindowState_ = false;
}

void MainWindow::setMiniPlayer(bool enabled) {
    if (enabled == miniPlayerActive_ && !miniPlayerAnimation_->isRunning())
        return;
    miniPlayerDesired_ = enabled;
    // Apply the mode change immediately. Window flags, geometry and chrome must
    // not depend on an animation completing: a suspended compositor or the
    // reduced-motion setting can skip it entirely, leaving the UI in the old
    // mode. The opacity fade stays purely decorative.
    applyMiniPlayerState(enabled);
    miniPlayerAnimation_->stopAt(0.0);
    animateMiniPlayerWindow(1.0);
}

void MainWindow::setMiniPlayerImmediately(bool enabled) {
    miniPlayerDesired_ = enabled;
    miniPlayerAnimation_->stopAt(1.0);
    applyMiniPlayerState(enabled);
}

void MainWindow::applyMiniPlayerState(bool enabled) {
    if (enabled == miniPlayerActive_)
        return;
    auto *action = findChild<QAction *>(QStringLiteral("miniPlayerAction"));
    hideThumbnailPreview();
    if (enabled) {
        if (isFullScreen())
            setFullscreen(false);
        miniPlayerActive_ = true;
        miniPlayerSavedGeometry_ = saveGeometry();
        miniPlayerGeometry_ = isMaximized() ? normalGeometry() : geometry();
        miniPlayerMaximized_ = isMaximized();
        showPlaylistAction_->setEnabled(false);
        playlistPane_->hide();
        menuBar()->hide();
        statusBar()->hide();
        setWindowFlag(Qt::Tool, true);
        setWindowFlag(Qt::WindowStaysOnTopHint, true);
        setWindowState(windowState() & ~Qt::WindowMaximized);
        resize(480, 320);
        show();
    } else {
        if (isFullScreen())
            setFullscreen(false);
        miniPlayerActive_ = false;
        menuBar()->show();
        statusBar()->show();
        showPlaylistAction_->setEnabled(true);
        setWindowFlag(Qt::Tool, false);
        setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTop_);
        show();
        if (miniPlayerGeometry_.isValid())
            setGeometry(miniPlayerGeometry_);
        if (miniPlayerMaximized_)
            showMaximized();
        playlistPane_->setVisible(showPlaylistAction_->isChecked());
        applyPlaylistWidth();
    }
    if (action) {
        action->setChecked(enabled);
        action->setText(enabled ? tr("退出小窗") : tr("小窗播放"));
        action->setToolTip(action->text());
        action->setStatusTip(action->text());
        setAppearanceIcon(action, enabled ? AppearanceIcons::Symbol::Remove
                                          : AppearanceIcons::Symbol::Restore);
    }
}

void MainWindow::animateMiniPlayerWindow(qreal opacity) { miniPlayerAnimation_->fadeTo(opacity); }

void MainWindow::setAlwaysOnTop(bool enabled) {
    if (enabled == alwaysOnTop_)
        return;
    alwaysOnTop_ = enabled;
    if (alwaysOnTopAction_) {
        const QSignalBlocker blocker(alwaysOnTopAction_);
        alwaysOnTopAction_->setChecked(enabled);
        const auto label = tr("窗口置顶");
        alwaysOnTopAction_->setText(label);
        alwaysOnTopAction_->setToolTip(label);
        alwaysOnTopAction_->setStatusTip(label);
    }
    // Fullscreen and mini-player own the top flag while active; apply the
    // user's choice when they exit without disturbing the current mode.
    if (isFullScreen() || miniPlayerActive_)
        return;
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, enabled);
    if (wasVisible)
        show();
}

void MainWindow::applyPlaylistWidth() {
    if (!splitter_ || playlistWidth_ <= 0)
        return;
    // The splitter has no layout geometry until the window is shown; use the
    // window width only for that initial restore.
    const int available = splitter_->isVisible() ? splitter_->width() : width();
    if (available <= 1)
        return;
    const int requested = std::clamp(playlistWidth_, 240, 1000);
    const int effective = std::min(requested, std::max(240, available - 1));
    splitter_->setSizes({available - effective, effective});
}

void MainWindow::rememberPlaylistWidth() {
    if (!playlistPane_)
        return;
    const int width = playlistPane_->width();
    if (width > 0)
        playlistWidth_ = std::clamp(width, 240, 1000);
}

void MainWindow::revealControls() {
    if (!isFullScreen())
        return;
    fullscreenControls_->reveal();
    video_->unsetCursor();
    controlsTimer_.start();
}

int MainWindow::interactionVolumeStep() const { return interactionPreferences_.wheelVolumeStep; }

void MainWindow::applyInteractionPreferences(const InteractionPreferences &preferences) {
    interactionPreferences_ = preferences.normalized();
    if (playlistPane_)
        playlistPane_->setSingleClickActivation(interactionPreferences_.playlistSingleClick);
    if (transport_)
        transport_->setTimeShowsRemaining(interactionPreferences_.showRemainingTime);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == video_ && event->type() == QEvent::Resize) {
        feedbackOverlay_->setGeometry(video_->rect());
        playbackOverlay_->setGeometry(video_->rect());
    }
    if (watched == playlistPane_ && event->type() == QEvent::Hide)
        rememberPlaylistWidth();
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::Wheel)
        revealControls();
    return QMainWindow::eventFilter(watched, event);
}
