#pragma once

#include <QMainWindow>
#include <QTimer>
#include <memory>

class MpvController;
class VideoWidget;
class TransportBar;
class TrackMenus;
class PlaylistController;
class PlaylistPane;
class QAction;
class QMenu;
class QLabel;

class MainWindow final : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(bool silentAudio = false);
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

  private:
    void createActions();
    void chooseFile();
    void chooseFilesToAppend();
    void updateState();
    void showError(const QString &message);
    void setFullscreen(bool enabled);
    void revealControls();
    std::unique_ptr<MpvController> controller_;
    std::unique_ptr<PlaylistController> playlist_;
    VideoWidget *video_ = nullptr;
    TransportBar *transport_ = nullptr;
    TrackMenus *trackMenus_ = nullptr;
    PlaylistPane *playlistPane_ = nullptr;
    QLabel *emptyHint_ = nullptr;
    QAction *openAction_ = nullptr;
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
    QTimer controlsTimer_;
    QStringList pendingPaths_;
    QByteArray normalGeometry_;
    bool normalMaximized_ = false;
};
