#include "playback/mpv/mpvcontroller.h"
#include "playback/playlist/playlistcontroller.h"
#include "playback/playlist/playlistmodel.h"
#include "session/recentmediahistory.h"
#include "session/sessionmanager.h"
#include "ui/menus/foldermenu.h"
#include "ui/panels/playlistpane.h"
#include "ui/panels/welcomepanel.h"
#include "ui/window/mainwindow.h"

#include <QAction>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QStandardPaths>
#include <QStatusBar>

void MainWindow::restoreSession(const QString &path) {
    if (path.isEmpty())
        return;
    const auto recentError = recentMedia_->load(path + QStringLiteral(".recent.json"));
    if (!recentError.isEmpty())
        QMetaObject::invokeMethod(
            this, [this, recentError] { statusBar()->showMessage(recentError); },
            Qt::QueuedConnection);
    session_ = std::make_unique<SessionManager>(*controller_, *playlist_, path);
    connect(
        session_.get(), &SessionManager::configurationErrorOccurred, this,
        [this](const QString &message) { statusBar()->showMessage(message); },
        Qt::QueuedConnection);
    const auto loaded = session_->restore();
    if (!loaded.error.isEmpty()) {
        statusBar()->showMessage(tr("无法恢复会话：%1").arg(loaded.error));
        return;
    }
    sessionReady_ = true;
    folderMenu_->setMediaLibraryPreferences(loaded.state.mediaLibrary);
    updateSeekActionLabels(loaded.state.seekStepSeconds);
    applyInteractionPreferences(session_->interactionPreferences());
    connect(session_.get(), &SessionManager::playbackHistoryChanged, this, [this] {
        if (welcomePanel_)
            welcomePanel_->setResumeEntries(session_->playbackHistory());
    });
    welcomePanel_->setResumeEntries(session_->playbackHistory());
    if (!loaded.state.window.geometry.isEmpty()) {
        restoreGeometry(loaded.state.window.geometry);
        // Fullscreen is a transient mode; only the normal/maximized window is durable.
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
    showPlaylistAction_->setChecked(loaded.state.window.playlistVisible);
    playlistWidth_ = loaded.state.window.playlistWidth;
    applyPlaylistWidth();
    setAlwaysOnTop(loaded.state.window.alwaysOnTop);
    if (!loaded.state.paths.isEmpty())
        statusBar()->showMessage(loaded.state.resumePosition > 0
                                     ? tr("已恢复播放列表 · 按空格从上次位置继续播放")
                                     : tr("已恢复播放列表 · 按空格开始播放"));
    setupSessionAutosave();
}

QString MainWindow::saveSession() {
    const auto recentError = recentMedia_->save();
    if (!session_)
        return recentError;
    WindowPreferences window;
    window.geometry = miniPlayerActive_ ? miniPlayerSavedGeometry_
                      : isFullScreen()  ? normalGeometry_
                                        : saveGeometry();
    window.playlistVisible = showPlaylistAction_->isChecked();
    if (playlistPane_->isVisible())
        rememberPlaylistWidth();
    window.playlistWidth = playlistWidth_;
    window.alwaysOnTop = alwaysOnTop_;
    const auto sessionError = session_->save(std::move(window), pendingPaths_);
    if (recentError.isEmpty())
        return sessionError;
    return sessionError.isEmpty() ? recentError : sessionError + QLatin1Char('\n') + recentError;
}

QString MainWindow::mediaDirectory() const {
    if (session_ && !session_->mediaDirectory().isEmpty())
        return session_->mediaDirectory();
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
}

int MainWindow::seekStepSeconds() const {
    return session_ ? session_->seekStepSeconds() : seekStepSecondsDefault_;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    folderMenu_->cancel();
    sessionAutosaveTimer_.stop();
    const auto error = saveSession();
    sessionSaveAttempted_ = true;
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("会话未保存"), error);
    QMainWindow::closeEvent(event);
}

// Playlist file pickers live with the other desktop/session integration code so
// the window coordination unit stays focused on playback state.
void MainWindow::choosePlaylistToOpen() {
    folderMenu_->cancel();
    const auto path =
        QFileDialog::getOpenFileName(this, tr("打开播放列表"), mediaDirectory(),
                                     tr("播放列表 (*.m3u8 *.m3u *.pls *.xspf);;M3U8 (*.m3u8 "
                                        "*.m3u);;PLS (*.pls);;XSPF (*.xspf);;所有文件 (*)"));
    if (!path.isEmpty())
        openPlaylist(path);
}

void MainWindow::choosePlaylistToSave() {
    auto path = QFileDialog::getSaveFileName(this, tr("保存播放列表"), mediaDirectory(),
                                             tr("M3U8 (*.m3u8 *.m3u);;PLS (*.pls);;XSPF (*.xspf)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".m3u8");
    savePlaylist(path);
}

void MainWindow::setupSessionAutosave() {
    if (!session_)
        return;
    sessionAutosaveTimer_.setSingleShot(true);
    const int override = qEnvironmentVariableIntValue("NEKOPLAYER_AUTOSAVE_MS");
    sessionAutosaveTimer_.setInterval(override > 0 ? override : 4000);
    connect(&sessionAutosaveTimer_, &QTimer::timeout, this, [this] {
        if (!session_ || !sessionReady_ || sessionSaveAttempted_)
            return;
        const auto error = saveSession();
        if (!error.isEmpty())
            statusBar()->showMessage(error, 5000);
    });
    connect(controller_.get(), &MpvController::stateChanged, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_.get(), &PlaylistController::currentChanged, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_.get(), &PlaylistController::navigationChanged, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_->model(), &PlaylistModel::rowsInserted, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_->model(), &PlaylistModel::rowsRemoved, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_->model(), &PlaylistModel::rowsMoved, this,
            &MainWindow::scheduleSessionAutosave);
    connect(playlist_->model(), &PlaylistModel::modelReset, this,
            &MainWindow::scheduleSessionAutosave);
}

void MainWindow::scheduleSessionAutosave() {
    if (session_ && sessionReady_ && !sessionSaveAttempted_ && !sessionAutosaveTimer_.isActive())
        sessionAutosaveTimer_.start();
}
