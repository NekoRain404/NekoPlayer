#pragma once

#include <QMainWindow>
#include <QPersistentModelIndex>
#include <QTimer>
#include <memory>

class MpvController;
class VideoWidget;
class TransportBar;
class TrackMenus;
class SessionManager;
class PlaylistController;
class PlaylistPane;
class QAction;
class QMenu;
class QLabel;
class LocalFileDrop;
class ActionFeedbackOverlay;

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

  signals:
    void userError(const QString &message);

  protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

  private:
    void createActions();
    void chooseFile();
    void chooseFilesToAppend();
    void choosePlaylistToOpen();
    void choosePlaylistToSave();
    void openPlaylist(const QString &path);
    void savePlaylist(const QString &path);
    void clearPlaylist();
    void updateState();
    void showError(const QString &message);
    void showActionFeedback(const QString &message, int durationMs = 2500);
    void setFullscreen(bool enabled);
    void revealControls();
    void restoreSession(const QString &path);
    QString saveSession();
    [[nodiscard]] QString mediaDirectory() const;
    std::unique_ptr<MpvController> controller_;
    std::unique_ptr<PlaylistController> playlist_;
    std::unique_ptr<SessionManager> session_;
    VideoWidget *video_ = nullptr;
    TransportBar *transport_ = nullptr;
    TrackMenus *trackMenus_ = nullptr;
    PlaylistPane *playlistPane_ = nullptr;
    LocalFileDrop *fileDrop_ = nullptr;
    QLabel *emptyHint_ = nullptr;
    ActionFeedbackOverlay *feedbackOverlay_ = nullptr;
    QAction *openAction_ = nullptr;
    QAction *openPlaylistAction_ = nullptr;
    QAction *savePlaylistAction_ = nullptr;
    QAction *pauseAction_ = nullptr;
    QAction *stopAction_ = nullptr;
    QAction *muteAction_ = nullptr;
    QAction *fullscreenAction_ = nullptr;
    QAction *appendAction_ = nullptr;
    QAction *previousAction_ = nullptr;
    QAction *nextAction_ = nullptr;
    QAction *clearPlaylistAction_ = nullptr;
    QAction *showPlaylistAction_ = nullptr;
    QMenu *contextMenu_ = nullptr;
    QMenu *playlistMenu_ = nullptr;
    QTimer controlsTimer_;
    QStringList pendingPaths_;
    QPersistentModelIndex pendingPlaylistIndex_;
    QByteArray normalGeometry_;
    bool normalMaximized_ = false;
    bool sessionSaveAttempted_ = false;
};
