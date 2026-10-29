#include "ui/menus/foldermenu.h"
#include "input/folderscanjob.h"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QWidget>
#include <utility>

FolderMenu::FolderMenu(QWidget *window, std::function<QString()> initialDirectory)
    : QObject(window), window_(window), menu_(new QMenu(tr("文件夹"), window)),
      recursiveAction_(new QAction(tr("包含子文件夹"), this)),
      cancelAction_(new QAction(tr("取消文件夹扫描"), this)), progress_(nullptr),
      job_(new FolderScanJob(this)), initialDirectory_(std::move(initialDirectory)) {
    menu_->setObjectName(QStringLiteral("folderMenu"));
    const auto add = [this, window](const QString &text, const char *name, QKeySequence shortcut,
                                    bool append) {
        auto *action = new QAction(text, this);
        action->setObjectName(QString::fromLatin1(name));
        action->setShortcut(shortcut);
        action->setAutoRepeat(false);
        window->addAction(action);
        menu_->addAction(action);
        connect(action, &QAction::triggered, this, [this, append] { chooseDirectory(append); });
        return action;
    };
    openAction_ =
        add(tr("打开文件夹…"), "openFolderAction", QKeySequence(Qt::CTRL | Qt::Key_D), false);
    appendAction_ = add(tr("添加文件夹…"), "appendFolderAction",
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D), true);
    menu_->addSeparator();
    recursiveAction_->setObjectName(QStringLiteral("recursiveFolderAction"));
    recursiveAction_->setCheckable(true);
    cancelAction_->setObjectName(QStringLiteral("cancelFolderScanAction"));
    cancelAction_->setEnabled(false);
    menu_->addActions({recursiveAction_, cancelAction_});
    connect(cancelAction_, &QAction::triggered, this, &FolderMenu::cancel);
    connect(recursiveAction_, &QAction::toggled, this, [this](bool recursive) {
        emit mediaLibraryPreferencesChanged(scanningEnabled_, recursive);
    });
    connect(job_, &FolderScanJob::scanningChanged, this, [this] {
        const bool scanning = job_->isScanning();
        cancelAction_->setEnabled(scanning);
        if (scanning) {
            ensureProgress()->reset();
            progress_->show();
        } else if (progress_) {
            progress_->hide();
        }
    });
    connect(job_, &FolderScanJob::completed, this, [this](const QStringList &paths) {
        if (paths.isEmpty()) {
            emit feedback(tr("文件夹中没有找到支持的音视频文件"));
            return;
        }
        emit filesReady(paths, append_);
    });
    connect(job_, &FolderScanJob::failed, this, &FolderMenu::errorOccurred);
}

QProgressDialog *FolderMenu::ensureProgress() {
    if (progress_)
        return progress_;
    // QProgressDialog starts its show timer from its constructor's initial
    // value. Create it lazily so no empty "scanning" window flashes at startup.
    progress_ = new QProgressDialog(tr("正在扫描媒体文件…"), tr("取消"), 0, 0, window_);
    progress_->setObjectName(QStringLiteral("folderScanProgress"));
    progress_->setWindowTitle(tr("扫描文件夹"));
    progress_->setWindowModality(Qt::NonModal);
    progress_->setMinimumDuration(0);
    progress_->setAutoClose(false);
    progress_->setAutoReset(false);
    connect(progress_, &QProgressDialog::canceled, this, &FolderMenu::cancel);
    return progress_;
}

QMenu *FolderMenu::menu() const { return menu_; }

bool FolderMenu::recursiveEnabled() const { return recursiveAction_->isChecked(); }

MediaLibraryPreferences FolderMenu::mediaLibraryPreferences() const {
    return {scanningEnabled_, recursiveAction_->isChecked()};
}

void FolderMenu::setMediaLibraryPreferences(MediaLibraryPreferences preferences) {
    scanningEnabled_ = preferences.scanningEnabled;
    openAction_->setEnabled(scanningEnabled_);
    appendAction_->setEnabled(scanningEnabled_);
    recursiveAction_->setEnabled(scanningEnabled_);
    if (!scanningEnabled_)
        cancel();
    // Applying the stored preference must not look like a user toggle: the
    // caller already owns persisting the session value.
    const QSignalBlocker blocker(recursiveAction_);
    recursiveAction_->setChecked(preferences.recursiveByDefault);
}

void FolderMenu::start(const QString &directory, bool append, bool recursive) {
    start(QStringList{directory}, append, recursive);
}

void FolderMenu::start(const QStringList &directories, bool append, bool recursive) {
    if (!scanningEnabled_) {
        emit feedback(tr("媒体文件夹扫描已关闭；可在偏好设置的媒体库中重新开启。"));
        return;
    }
    append_ = append;
    job_->start(directories, recursive);
}

void FolderMenu::cancel() { job_->cancel(); }

void FolderMenu::chooseDirectory(bool append) {
    // Choosing a new source invalidates an older scan before entering the modal chooser.
    cancel();
    const auto directory = QFileDialog::getExistingDirectory(
        window_, append ? tr("添加文件夹") : tr("打开文件夹"), initialDirectory_());
    if (!directory.isEmpty())
        start(directory, append, recursiveAction_->isChecked());
}
