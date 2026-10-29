#pragma once

#include <QDialog>
#include <QStringList>
#include <functional>

class MediaLibrary;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QSortFilterProxyModel;
class QStandardItemModel;
class QTreeView;

// Modeless media library browser. All playback intent is emitted as signals so
// this dialog never talks to the playback controller or mpv directly.
class LibraryDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit LibraryDialog(MediaLibrary &library, QWidget *parent = nullptr);

  signals:
    void playRequested(const QString &path);
    void appendRequested(const QStringList &paths);

  private:
    void rebuildFolders();
    void rebuildEntries();
    void chooseFolder();
    void removeSelectedFolder();
    void refresh();
    void playSelected();
    void appendSelected();
    void updateCount();
    void updateEmptyState();
    void updateSelectionActions();
    void updateScanControls();
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QStringList selectedPaths() const;

    MediaLibrary &library_;
    QListWidget *folderList_ = nullptr;
    QPushButton *addFolderButton_ = nullptr;
    QPushButton *removeFolderButton_ = nullptr;
    QCheckBox *recursiveCheck_ = nullptr;
    QLabel *memoryHint_ = nullptr;
    QLineEdit *search_ = nullptr;
    QTreeView *mediaView_ = nullptr;
    QStandardItemModel *mediaModel_ = nullptr;
    QSortFilterProxyModel *filter_ = nullptr;
    QLabel *countLabel_ = nullptr;
    QLabel *emptyLabel_ = nullptr;
    QLabel *statusLabel_ = nullptr;
    QProgressBar *progress_ = nullptr;
    QPushButton *refreshButton_ = nullptr;
    QPushButton *playButton_ = nullptr;
    QPushButton *appendButton_ = nullptr;
    QPushButton *closeButton_ = nullptr;
    QString lastDirectory_;
};

namespace LibraryDialogSupport {
// Lazily creates and shows the dialog owned by `host`, reusing both the dialog
// and its MediaLibrary on later calls. The library path follows AppConfigLocation.
LibraryDialog *show(QWidget *host, std::function<void(const QString &)> playRequested,
                    std::function<void(const QStringList &)> appendRequested);
} // namespace LibraryDialogSupport
