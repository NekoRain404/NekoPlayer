#include "mainwindow.h"
#include "actionfeedbackoverlay.h"
#include "actionfeedbacktext.h"
#include "appearanceicons.h"
#include "foldermenu.h"
#include "fullscreencontrols.h"
#include "input/localfiledrop.h"
#include "networkopendialog.h"
#include "playback/mediasource.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "playbackoverlay.h"
#include "playlist/m3u8store.h"
#include "playlistpane.h"
#include "preview/thumbnailservice.h"
#include "session/recentmediahistory.h"
#include "session/sessionmanager.h"
#include "thumbnailpreview.h"
#include "thumbnailpreviewpolicy.h"
#include "transportbar.h"
#include "welcomepanel.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QScreen>
#include <QStatusBar>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
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
    resize(1100, 680);
    setMinimumSize(560, 360);
    setAcceptDrops(true);
    fileDrop_ = new LocalFileDrop(*controller_, this);
    connect(fileDrop_, &LocalFileDrop::mediaFilesDropped, this, &MainWindow::appendFiles);
    connect(fileDrop_, &LocalFileDrop::errorOccurred, this, &MainWindow::showError);
    createActions();
    connect(fileDrop_, &LocalFileDrop::foldersDropped, this, [this](const QStringList &paths) {
        folderMenu_->start(paths, true, folderMenu_->recursiveEnabled());
    });
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    centralLayout_ = layout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    video_ = new VideoWidget(*controller_, content);
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
    playlistPane_->hide();
    auto *body = new QWidget(content);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(video_, 1);
    bodyLayout->addWidget(playlistPane_);
    transport_ = new TransportBar(openAction_, previousAction_, pauseAction_, nextAction_,
                                  stopAction_, muteAction_, fullscreenAction_, content);
    thumbnailService_ = new ThumbnailService(this);
    thumbnailPreview_ = new ThumbnailPreview(this);
    fullscreenControls_ = new FullscreenControls(*transport_, *video_, *centralLayout_, this);
    layout->addWidget(body, 1);
    layout->addWidget(transport_);
    setCentralWidget(content);
    statusBar()->setSizeGripEnabled(false);
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
    connect(video_, &VideoWidget::doubleClicked, fullscreenAction_, &QAction::trigger);
    connect(video_, &VideoWidget::ready, this, [this] {
        const auto pendingPlaylistIndex =
            std::exchange(pendingPlaylistIndex_, QPersistentModelIndex{});
        if (pendingPlaylistIndex.isValid())
            playlist_->playAt(pendingPlaylistIndex.row());
        if (!pendingPaths_.isEmpty()) {
            const auto paths = std::exchange(pendingPaths_, {});
            playlist_->addFiles(paths);
        } else if (!pendingPlaylistIndex.isValid())
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
    restoreSession(sessionPath);
    updateState();
    if (!controller_->isReady())
        showError(controller_->errorString());
}

MainWindow::~MainWindow() {
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

void MainWindow::openFile(const QString &path) { openFiles({path}); }

void MainWindow::openFiles(const QStringList &paths) {
    folderMenu_->cancel();
    if (paths.isEmpty())
        return;
    const auto sources = normalizedSources(paths);
    if (sources.isEmpty()) {
        showError(tr("媒体地址无效；网络地址需要包含受支持的协议与主机名"));
        return;
    }
    if (video_->hasFailed()) {
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

void MainWindow::choosePlaylistToOpen() {
    folderMenu_->cancel();
    const auto path = QFileDialog::getOpenFileName(this, tr("打开播放列表"), mediaDirectory(),
                                                   tr("M3U8 播放列表 (*.m3u8);;所有文件 (*)"));
    if (!path.isEmpty())
        openPlaylist(path);
}

void MainWindow::choosePlaylistToSave() {
    auto path = QFileDialog::getSaveFileName(this, tr("保存播放列表"), mediaDirectory(),
                                             tr("M3U8 播放列表 (*.m3u8)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".m3u8");
    savePlaylist(path);
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
    pendingPaths_.clear();
    pendingPlaylistIndex_ = QPersistentModelIndex{};
    playlist_->restoreFiles(loaded.paths, loaded.currentIndex);
    if (video_->isReady() && loaded.currentIndex >= 0)
        playlist_->playAt(loaded.currentIndex);
    else if (loaded.currentIndex >= 0)
        pendingPlaylistIndex_ = playlist_->model()->index(loaded.currentIndex);
    updateState();
}

void MainWindow::savePlaylist(const QString &path) {
    const auto error =
        M3u8Store::save(path, playlist_->model()->paths(), playlist_->model()->currentIndex());
    if (!error.isEmpty())
        showError(error);
    else
        showActionFeedback(tr("播放列表已保存"));
}

void MainWindow::chooseFile() {
    folderMenu_->cancel();
    const auto path =
        QFileDialog::getOpenFileName(this, tr("打开媒体文件"), mediaDirectory(),
                                     tr("媒体文件 (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts *.mp3 "
                                        "*.flac *.wav);;所有文件 (*)"));
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
    const auto paths =
        QFileDialog::getOpenFileNames(this, tr("添加媒体文件"), mediaDirectory(),
                                      tr("媒体文件 (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts "
                                         "*.mp3 *.flac *.wav);;所有文件 (*)"));
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
    setWindowTitle(state.path.isEmpty()
                       ? QStringLiteral("NekoPlayer")
                       : MediaSource::displayName(state.path) + QStringLiteral(" — NekoPlayer"));
}

void MainWindow::showError(const QString &message) {
    feedbackOverlay_->clearMessage();
    statusBar()->showMessage(tr("无法播放：%1").arg(message));
    if (isFullScreen()) {
        revealControls();
        feedbackOverlay_->showMessage(tr("无法播放：%1").arg(message), 5000);
    }
    emit userError(message);
}

void MainWindow::showActionFeedback(const QString &message, int durationMs) {
    if (isFullScreen())
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

    if (enabled && miniPlayerActive_)
        setMiniPlayer(false);
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
        }
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
    auto *action = findChild<QAction *>(QStringLiteral("miniPlayerAction"));
    if (enabled == miniPlayerActive_)
        return;
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
        setWindowFlag(Qt::WindowStaysOnTopHint, false);
        show();
        if (miniPlayerGeometry_.isValid())
            setGeometry(miniPlayerGeometry_);
        if (miniPlayerMaximized_)
            showMaximized();
        playlistPane_->setVisible(showPlaylistAction_->isChecked());
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

void MainWindow::revealControls() {
    if (!isFullScreen())
        return;
    fullscreenControls_->reveal();
    video_->unsetCursor();
    controlsTimer_.start();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == video_ && event->type() == QEvent::Resize) {
        feedbackOverlay_->setGeometry(video_->rect());
        playbackOverlay_->setGeometry(video_->rect());
    }
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)
        revealControls();
    return QMainWindow::eventFilter(watched, event);
}
