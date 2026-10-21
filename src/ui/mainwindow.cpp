#include "mainwindow.h"
#include "playback/mpvcontroller.h"
#include "playback/playlistcontroller.h"
#include "playback/playlistmodel.h"
#include "playback/videowidget.h"
#include "playlistpane.h"
#include "session/sessionmanager.h"
#include "transportbar.h"

#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QStatusBar>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

MainWindow::MainWindow(bool silentAudio, const QString &sessionPath)
    : controller_(std::make_unique<MpvController>(false, silentAudio)),
      playlist_(std::make_unique<PlaylistController>(*controller_)) {
    setWindowTitle(QStringLiteral("NekoPlayer"));
    resize(1100, 680);
    setMinimumSize(560, 360);
    setAcceptDrops(true);
    setStyleSheet(QStringLiteral(
        "QMainWindow, QWidget#transportBar { background: #202126; color: #e3e4e9; }"
        "QMenuBar, QMenu, QStatusBar { background: #26282e; color: #d7d9e0; }"
        "QMenuBar::item:selected, QMenu::item:selected { background: #414655; }"
        "QLabel { color: #bfc3cf; }"
        "QToolButton { color: #e3e4e9; background: transparent; border: none; border-radius: 4px; }"
        "QToolButton:hover { background: #3b3f4b; }"
        "QToolButton:disabled { color: #666a75; }"
        "QSlider::groove:horizontal { height: 4px; background: #454953; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #8ba9ef; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #dce5fa; width: 10px; margin: -4px 0; "
        "border-radius: 5px; }"));
    createActions();
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    video_ = new VideoWidget(*controller_, content);
    emptyHint_ = new QLabel(tr("NekoPlayer\n\n拖入视频，或按 Ctrl+O 打开文件"), video_);
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyHint_->setStyleSheet(
        QStringLiteral("color: #8b92a3; font-size: 18px; background: transparent;"));
    auto *videoLayout = new QVBoxLayout(video_);
    videoLayout->addWidget(emptyHint_);
    playlistPane_ = new PlaylistPane(playlist_->model(), appendAction_, previousAction_,
                                     nextAction_, clearPlaylistAction_, content);
    playlistPane_->hide();
    auto *body = new QWidget(content);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(video_, 1);
    bodyLayout->addWidget(playlistPane_);
    transport_ = new TransportBar(openAction_, previousAction_, pauseAction_, nextAction_,
                                  stopAction_, muteAction_, fullscreenAction_, content);
    layout->addWidget(body, 1);
    layout->addWidget(transport_);
    setCentralWidget(content);
    statusBar()->setSizeGripEnabled(false);
    statusBar()->showMessage(tr("就绪 · Qt 6 + libmpv"));
    connect(controller_.get(), &MpvController::stateChanged, this, &MainWindow::updateState);
    connect(controller_.get(), &MpvController::errorOccurred, this, &MainWindow::showError);
    connect(controller_.get(), &MpvController::fileLoaded, this, [this] {
        statusBar()->showMessage(
            tr("正在播放 · %1").arg(QFileInfo(controller_->state().path).fileName()));
    });
    connect(playlist_.get(), &PlaylistController::queueFinished, this,
            [this] { statusBar()->showMessage(tr("播放列表已结束")); });
    connect(playlist_.get(), &PlaylistController::queueError, this, &MainWindow::showError);
    connect(playlist_.get(), &PlaylistController::currentChanged, this, &MainWindow::updateState);
    connect(playlist_.get(), &PlaylistController::repeatModeChanged, this,
            &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::rowsInserted, this, &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::rowsRemoved, this, &MainWindow::updateState);
    connect(playlist_->model(), &PlaylistModel::modelReset, this, &MainWindow::updateState);
    connect(playlistPane_, &PlaylistPane::playRequested, playlist_.get(), [this](int row) {
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
        if (!pendingPaths_.isEmpty()) {
            const auto paths = std::exchange(pendingPaths_, {});
            playlist_->addFiles(paths);
        } else
            controller_->resumeAfterRendererReset();
        updateState();
    });
    connect(transport_, &TransportBar::seekRequested, controller_.get(),
            &MpvController::seekAbsolute);
    connect(transport_, &TransportBar::volumeRequested, controller_.get(),
            &MpvController::setVolume);
    controlsTimer_.setSingleShot(true);
    controlsTimer_.setInterval(2200);
    connect(&controlsTimer_, &QTimer::timeout, this, [this] {
        if (isFullScreen() && !transport_->underMouse() && !contextMenu_->isVisible()) {
            transport_->hide();
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
    controlsTimer_.stop();
    if (!sessionSaveAttempted_) {
        if (const auto error = saveSession(); !error.isEmpty())
            qWarning().noquote() << error;
    }
    // QMainWindow deletes children after members. GPU resources must go first.
    delete takeCentralWidget();
}
MpvController *MainWindow::controller() const { return controller_.get(); }
VideoWidget *MainWindow::videoWidget() const { return video_; }
PlaylistController *MainWindow::playlist() const { return playlist_.get(); }

void MainWindow::openFile(const QString &path) { openFiles({path}); }

void MainWindow::openFiles(const QStringList &paths) {
    if (paths.isEmpty())
        return;
    if (video_->hasFailed()) {
        showError(tr("OpenGL 初始化失败，无法打开媒体文件"));
        return;
    }
    clearPlaylist();
    appendFiles(paths);
}

void MainWindow::appendFiles(const QStringList &paths) {
    QStringList absolutePaths;
    for (const auto &path : paths) {
        if (!path.isEmpty())
            absolutePaths.append(QFileInfo(path).absoluteFilePath());
    }
    if (absolutePaths.isEmpty())
        return;
    if (session_)
        session_->rememberMedia(absolutePaths.constFirst());
    if (video_->isReady())
        playlist_->addFiles(absolutePaths);
    else
        pendingPaths_.append(absolutePaths);
    updateState();
}

void MainWindow::clearPlaylist() {
    pendingPaths_.clear();
    playlist_->clear();
}

void MainWindow::chooseFile() {
    const auto path =
        QFileDialog::getOpenFileName(this, tr("打开媒体文件"), mediaDirectory(),
                                     tr("媒体文件 (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts *.mp3 "
                                        "*.flac *.wav);;所有文件 (*)"));
    if (!path.isEmpty())
        openFile(path);
}

void MainWindow::chooseFilesToAppend() {
    const auto paths =
        QFileDialog::getOpenFileNames(this, tr("添加媒体文件"), mediaDirectory(),
                                      tr("媒体文件 (*.mkv *.mp4 *.avi *.mov *.webm *.m4v *.ts "
                                         "*.mp3 *.flac *.wav);;所有文件 (*)"));
    appendFiles(paths);
}

void MainWindow::updateState() {
    const auto &state = controller_->state();
    pauseAction_->setEnabled(video_->isReady() &&
                             (state.active || playlist_->model()->rowCount() > 0));
    stopAction_->setEnabled(state.active);
    const bool canPlay = !state.active || state.paused;
    pauseAction_->setText(canPlay ? tr("播放") : tr("暂停"));
    pauseAction_->setIcon(
        style()->standardIcon(canPlay ? QStyle::SP_MediaPlay : QStyle::SP_MediaPause));
    muteAction_->setChecked(state.muted);
    muteAction_->setIcon(
        style()->standardIcon(state.muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
    previousAction_->setEnabled(video_->isReady() && playlist_->hasPrevious());
    nextAction_->setEnabled(video_->isReady() && playlist_->hasNext());
    clearPlaylistAction_->setEnabled(playlist_->model()->rowCount() > 0 ||
                                     !pendingPaths_.isEmpty());
    const auto currentRow = playlist_->model()->currentIndex();
    playlistPane_->view()->setCurrentIndex(currentRow >= 0 ? playlist_->model()->index(currentRow)
                                                           : QModelIndex{});
    transport_->present(state);
    emptyHint_->setVisible(!state.active);
    setWindowTitle(state.path.isEmpty()
                       ? QStringLiteral("NekoPlayer")
                       : QFileInfo(state.path).fileName() + QStringLiteral(" — NekoPlayer"));
}

void MainWindow::showError(const QString &message) {
    statusBar()->showMessage(tr("无法播放：%1").arg(message));
    if (isFullScreen()) {
        revealControls();
        statusBar()->show();
    }
    emit userError(message);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event) {
    const auto urls = event->mimeData()->urls();
    QStringList paths;
    for (const auto &url : urls) {
        if (!url.isLocalFile()) {
            showError(tr("播放列表仅支持本地媒体文件"));
            event->ignore();
            return;
        }
        paths.append(url.toLocalFile());
    }
    if (paths.isEmpty()) {
        showError(tr("请拖入本地媒体文件"));
        event->ignore();
        return;
    }
    appendFiles(paths);
    event->acceptProposedAction();
}

void MainWindow::setFullscreen(bool enabled) {
    if (enabled == isFullScreen())
        return;
    if (enabled) {
        normalGeometry_ = saveGeometry();
        normalMaximized_ = isMaximized();
        menuBar()->hide();
        statusBar()->hide();
        transport_->hide();
        showFullScreen();
        video_->setCursor(Qt::BlankCursor);
    } else {
        controlsTimer_.stop();
        video_->unsetCursor();
        showNormal();
        restoreGeometry(normalGeometry_);
        if (normalMaximized_)
            showMaximized();
        menuBar()->show();
        statusBar()->show();
        transport_->show();
    }
    fullscreenAction_->setChecked(enabled);
}

void MainWindow::revealControls() {
    if (!isFullScreen())
        return;
    transport_->show();
    video_->unsetCursor();
    controlsTimer_.start();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)
        revealControls();
    return QMainWindow::eventFilter(watched, event);
}
