#pragma once

#include "session/sessionstate.h"
#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QTimer>
#include <memory>

class MpvController;
class VideoWidget;
class TransportBar;
class TrackMenus;
class SessionManager;
class PlaylistController;
class PlaylistPane;
class PlaylistMaintenanceMenu;
class QAction;
class QMenu;
class QLabel;
class WelcomePanel;
class LocalFileDrop;
class ActionFeedbackOverlay;
class PlaybackOverlay;
class FolderMenu;
class RecentMediaHistory;
class FullscreenControls;
class PreferencesCoordinator;
class QVBoxLayout;
class QSplitter;
class ThumbnailPreview;
class ThumbnailService;
class WindowOpacityTransition;
struct ThumbnailResult;
namespace AppearanceIcons {
enum class Symbol;
}

class MainWindow final : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(bool silentAudio = false, const QString &sessionPath = {});
    ~MainWindow() override;
    [[nodiscard]] MpvController *controller() const;
    [[nodiscard]] VideoWidget *videoWidget() const;
    [[nodiscard]] PlaylistController *playlist() const;
    void openFile(const QString &path);
    void openFiles(const QStringList &paths);
    void appendFiles(const QStringList &paths);
    void openFolder(const QString &directory, bool append = false, bool recursive = false);
    void setStartPaused(bool paused);
    void enterFullscreen();

  signals:
    void userError(const QString &message);

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

  private:
    friend class WindowTest;

    void createActions();
    void updateSeekActionLabels(int seconds);
    void setAppearanceIcon(QAction *action, AppearanceIcons::Symbol symbol);
    void refreshAppearanceIcons();
    void chooseFile();
    void chooseNetworkUrl();
    void chooseFilesToAppend();
    void choosePlaylistToOpen();
    void choosePlaylistToSave();
    void openPlaylist(const QString &path);
    void savePlaylist(const QString &path);
    void clearPlaylist();
    void updateState();
    void showError(const QString &message);
    void showActionFeedback(const QString &message, int durationMs = 2500);
    void showSeekFeedback(const QString &message);
    void setFullscreen(bool enabled);
    void setMiniPlayer(bool enabled);
    void setMiniPlayerImmediately(bool enabled);
    void applyMiniPlayerState(bool enabled);
    void animateMiniPlayerWindow(qreal opacity);
    void setAlwaysOnTop(bool enabled);
    void applyPlaylistWidth();
    void rememberPlaylistWidth();
    void revealControls();
    void applyInteractionPreferences(const InteractionPreferences &preferences);
    void restoreSession(const QString &path);
    QString saveSession();
    void requestThumbnailPreview(double seconds, QPoint globalPosition);
    void hideThumbnailPreview();
    void showThumbnailPreview(quint64 requestId, const QString &source, double requestedSeconds,
                              ThumbnailResult result);
    [[nodiscard]] QString mediaDirectory() const;
    [[nodiscard]] int seekStepSeconds() const;
    [[nodiscard]] int interactionVolumeStep() const;
    std::unique_ptr<MpvController> controller_;
    std::unique_ptr<PlaylistController> playlist_;
    std::unique_ptr<SessionManager> session_;
    std::unique_ptr<RecentMediaHistory> recentMedia_;
    PreferencesCoordinator *preferencesCoordinator_ = nullptr;
    VideoWidget *video_ = nullptr;
    TransportBar *transport_ = nullptr;
    FullscreenControls *fullscreenControls_ = nullptr;
    QVBoxLayout *centralLayout_ = nullptr;
    TrackMenus *trackMenus_ = nullptr;
    PlaylistPane *playlistPane_ = nullptr;
    QSplitter *splitter_ = nullptr;
    int playlistWidth_ = 0;
    LocalFileDrop *fileDrop_ = nullptr;
    WelcomePanel *welcomePanel_ = nullptr;
    ActionFeedbackOverlay *feedbackOverlay_ = nullptr;
    PlaybackOverlay *playbackOverlay_ = nullptr;
    ThumbnailService *thumbnailService_ = nullptr;
    ThumbnailPreview *thumbnailPreview_ = nullptr;
    QPoint thumbnailAnchor_;
    FolderMenu *folderMenu_ = nullptr;
    QAction *openAction_ = nullptr;
    QAction *openNetworkAction_ = nullptr;
    QAction *openPlaylistAction_ = nullptr;
    QAction *savePlaylistAction_ = nullptr;
    QAction *pauseAction_ = nullptr;
    QAction *stopAction_ = nullptr;
    QAction *seekBackAction_ = nullptr;
    QAction *seekForwardAction_ = nullptr;
    QAction *muteAction_ = nullptr;
    QAction *fullscreenAction_ = nullptr;
    QAction *exitFullscreenAction_ = nullptr;
    QAction *appendAction_ = nullptr;
    QAction *previousAction_ = nullptr;
    QAction *nextAction_ = nullptr;
    QAction *clearPlaylistAction_ = nullptr;
    QAction *showPlaylistAction_ = nullptr;
    QAction *alwaysOnTopAction_ = nullptr;
    QMenu *contextMenu_ = nullptr;
    QMenu *playlistMenu_ = nullptr;
    QMenu *helpMenu_ = nullptr;
    PlaylistMaintenanceMenu *playlistMaintenance_ = nullptr;
    QTimer controlsTimer_;
    WindowOpacityTransition *miniPlayerAnimation_ = nullptr;
    QString lastTitlePath_;
    QStringList pendingPaths_;
    QPersistentModelIndex pendingPlaylistIndex_;
    QByteArray normalGeometry_;
    QByteArray miniPlayerSavedGeometry_;
    bool normalMaximized_ = false;
    bool fullscreenPresentationActive_ = false;
    bool syncingWindowState_ = false;
    QRect miniPlayerGeometry_;
    bool miniPlayerMaximized_ = false;
    bool miniPlayerActive_ = false;
    bool miniPlayerDesired_ = false;
    bool alwaysOnTop_ = false;
    bool startPaused_ = false;
    bool sessionSaveAttempted_ = false;
    // Session-only fallbacks used when --no-config leaves session_ null.
    bool resumePlaybackPositionDefault_ = true;
    int seekStepSecondsDefault_ = 5;
    InteractionPreferences interactionPreferences_;
    LanguagePreferences languagePreferences_;
};
