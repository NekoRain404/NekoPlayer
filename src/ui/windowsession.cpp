#include "foldermenu.h"
#include "mainwindow.h"
#include "playlistpane.h"
#include "session/recentmediahistory.h"
#include "session/sessionmanager.h"
#include "welcomepanel.h"

#include <QAction>
#include <QCloseEvent>
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
    if (!loaded.state.paths.isEmpty())
        statusBar()->showMessage(loaded.state.resumePosition > 0
                                     ? tr("已恢复播放列表 · 按空格从上次位置继续播放")
                                     : tr("已恢复播放列表 · 按空格开始播放"));
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

void MainWindow::closeEvent(QCloseEvent *event) {
    folderMenu_->cancel();
    const auto error = saveSession();
    sessionSaveAttempted_ = true;
    if (!error.isEmpty())
        QMessageBox::warning(this, tr("会话未保存"), error);
    QMainWindow::closeEvent(event);
}
