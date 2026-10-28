#include "librarydialog.h"

#include "appearancetokens.h"
#include "library/medialibrary.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <functional>
#include <utility>

namespace {
bool configurationEnabled() {
    const auto *application = QCoreApplication::instance();
    if (!application)
        return true;
    const auto value = application->property("configurationEnabled");
    return !value.isValid() || value.toBool();
}
} // namespace

LibraryDialog::LibraryDialog(MediaLibrary &library, QWidget *parent)
    : QDialog(parent), library_(library) {
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    setObjectName(QStringLiteral("mediaLibraryDialog"));
    setWindowTitle(tr("媒体库"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    resize(820, 560);
    setMinimumSize(620, 420);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(tokens.spaceUnit * 4, tokens.spaceUnit * 4, tokens.spaceUnit * 4,
                              tokens.spaceUnit * 4);
    outer->setSpacing(tokens.spaceUnit * 4);

    auto *leftPanel = new QWidget(this);
    auto *left = new QVBoxLayout(leftPanel);
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(tokens.spaceUnit * 2);
    auto *foldersLabel = new QLabel(tr("监视文件夹"), leftPanel);
    left->addWidget(foldersLabel);
    folderList_ = new QListWidget(leftPanel);
    folderList_->setObjectName(QStringLiteral("libraryFolderList"));
    folderList_->setAccessibleName(tr("监视文件夹列表"));
    folderList_->setSelectionMode(QAbstractItemView::SingleSelection);
    folderList_->setMinimumHeight(tokens.rowHeight * 8);
    left->addWidget(folderList_, 1);
    auto *folderButtons = new QHBoxLayout;
    folderButtons->setSpacing(tokens.spaceUnit * 2);
    addFolderButton_ = new QPushButton(tr("添加文件夹…"), leftPanel);
    addFolderButton_->setObjectName(QStringLiteral("libraryAddFolderButton"));
    addFolderButton_->setAccessibleName(tr("添加媒体文件夹"));
    removeFolderButton_ = new QPushButton(tr("移除"), leftPanel);
    removeFolderButton_->setObjectName(QStringLiteral("libraryRemoveFolderButton"));
    removeFolderButton_->setAccessibleName(tr("移除选中的媒体文件夹"));
    folderButtons->addWidget(addFolderButton_);
    folderButtons->addWidget(removeFolderButton_);
    folderButtons->addStretch();
    left->addLayout(folderButtons);
    recursiveCheck_ = new QCheckBox(tr("包含子文件夹"), leftPanel);
    recursiveCheck_->setObjectName(QStringLiteral("libraryRecursiveCheck"));
    recursiveCheck_->setAccessibleName(tr("扫描时包含子文件夹"));
    recursiveCheck_->setChecked(library_.recursive());
    left->addWidget(recursiveCheck_);
    memoryHint_ = new QLabel(tr("当前为临时模式：媒体库更改不会保存。"), leftPanel);
    memoryHint_->setObjectName(QStringLiteral("libraryMemoryOnlyHint"));
    memoryHint_->setWordWrap(true);
    memoryHint_->setVisible(!configurationEnabled());
    left->addWidget(memoryHint_);
    left->addStretch();
    leftPanel->setMinimumWidth(220);
    leftPanel->setMaximumWidth(340);
    outer->addWidget(leftPanel, 1);

    auto *rightPanel = new QWidget(this);
    auto *right = new QVBoxLayout(rightPanel);
    right->setContentsMargins(0, 0, 0, 0);
    right->setSpacing(tokens.spaceUnit * 2);
    search_ = new QLineEdit(rightPanel);
    search_->setObjectName(QStringLiteral("librarySearchEdit"));
    search_->setPlaceholderText(tr("搜索媒体库"));
    search_->setAccessibleName(tr("搜索媒体库"));
    search_->setClearButtonEnabled(true);
    search_->setMinimumHeight(tokens.controlHeight);
    right->addWidget(search_);

    mediaModel_ = new QStandardItemModel(this);
    mediaModel_->setHorizontalHeaderLabels({tr("名称"), tr("文件夹"), tr("路径")});
    filter_ = new QSortFilterProxyModel(this);
    filter_->setSourceModel(mediaModel_);
    filter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filter_->setFilterKeyColumn(-1);
    mediaView_ = new QTreeView(rightPanel);
    mediaView_->setObjectName(QStringLiteral("libraryMediaView"));
    mediaView_->setAccessibleName(tr("媒体库条目"));
    mediaView_->setModel(filter_);
    mediaView_->setRootIsDecorated(false);
    mediaView_->setUniformRowHeights(true);
    mediaView_->setAlternatingRowColors(true);
    mediaView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mediaView_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mediaView_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mediaView_->setTextElideMode(Qt::ElideMiddle);
    mediaView_->setMinimumHeight(tokens.rowHeight * 8);
    mediaView_->setColumnHidden(2, true);
    mediaView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    mediaView_->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    mediaView_->header()->resizeSection(1, 220);
    right->addWidget(mediaView_, 1);

    countLabel_ = new QLabel(rightPanel);
    countLabel_->setObjectName(QStringLiteral("libraryCountLabel"));
    countLabel_->setAccessibleName(tr("媒体库条目数量"));
    right->addWidget(countLabel_);
    emptyLabel_ = new QLabel(rightPanel);
    emptyLabel_->setObjectName(QStringLiteral("libraryEmptyHint"));
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    right->addWidget(emptyLabel_);
    progress_ = new QProgressBar(rightPanel);
    progress_->setObjectName(QStringLiteral("libraryProgress"));
    progress_->setAccessibleName(tr("媒体库扫描进度"));
    progress_->setTextVisible(true);
    progress_->setVisible(false);
    right->addWidget(progress_);
    statusLabel_ = new QLabel(rightPanel);
    statusLabel_->setObjectName(QStringLiteral("libraryStatusLabel"));
    statusLabel_->setWordWrap(true);
    statusLabel_->setTextFormat(Qt::PlainText);
    right->addWidget(statusLabel_);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(tokens.spaceUnit * 2);
    refreshButton_ = new QPushButton(tr("刷新"), rightPanel);
    refreshButton_->setObjectName(QStringLiteral("libraryRefreshButton"));
    playButton_ = new QPushButton(tr("播放"), rightPanel);
    playButton_->setObjectName(QStringLiteral("libraryPlayButton"));
    playButton_->setAccessibleName(tr("播放选中的媒体"));
    appendButton_ = new QPushButton(tr("添加到播放列表"), rightPanel);
    appendButton_->setObjectName(QStringLiteral("libraryAppendButton"));
    appendButton_->setAccessibleName(tr("将选中媒体添加到播放列表"));
    closeButton_ = new QPushButton(tr("关闭"), rightPanel);
    closeButton_->setObjectName(QStringLiteral("libraryCloseButton"));
    closeButton_->setAccessibleName(tr("关闭媒体库"));
    for (auto *button : {refreshButton_, playButton_, appendButton_, closeButton_}) {
        button->setAutoDefault(false);
        button->setMinimumHeight(tokens.controlHeight);
    }
    buttons->addWidget(refreshButton_);
    buttons->addWidget(playButton_);
    buttons->addWidget(appendButton_);
    buttons->addStretch();
    buttons->addWidget(closeButton_);
    right->addLayout(buttons);
    outer->addWidget(rightPanel, 3);

    connect(folderList_, &QListWidget::currentItemChanged, this, [this] { updateScanControls(); });
    connect(addFolderButton_, &QPushButton::clicked, this, &LibraryDialog::chooseFolder);
    connect(removeFolderButton_, &QPushButton::clicked, this, &LibraryDialog::removeSelectedFolder);
    connect(recursiveCheck_, &QCheckBox::toggled, this, [this](bool recursive) {
        library_.setRecursive(recursive);
        if (const auto error = library_.save(); !error.isEmpty())
            statusLabel_->setText(error);
    });
    connect(search_, &QLineEdit::textChanged, this, [this] {
        filter_->setFilterFixedString(search_->text().trimmed());
        updateCount();
        updateEmptyState();
    });
    connect(mediaView_, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto source = filter_->mapToSource(index);
        if (const auto *item = mediaModel_->item(source.row(), 0))
            emit playRequested(item->data(Qt::UserRole).toString());
    });
    if (auto *selection = mediaView_->selectionModel()) {
        connect(selection, &QItemSelectionModel::selectionChanged, this,
                [this] { updateSelectionActions(); });
    }
    connect(refreshButton_, &QPushButton::clicked, this, &LibraryDialog::refresh);
    connect(playButton_, &QPushButton::clicked, this, &LibraryDialog::playSelected);
    connect(appendButton_, &QPushButton::clicked, this, &LibraryDialog::appendSelected);
    connect(closeButton_, &QPushButton::clicked, this, &QDialog::close);

    connect(&library_, &MediaLibrary::foldersChanged, this, &LibraryDialog::rebuildFolders);
    connect(&library_, &MediaLibrary::entriesChanged, this, &LibraryDialog::rebuildEntries);
    connect(&library_, &MediaLibrary::recursiveChanged, this, [this](bool recursive) {
        const QSignalBlocker blocker(recursiveCheck_);
        recursiveCheck_->setChecked(recursive);
    });
    connect(&library_, &MediaLibrary::scanStarted, this, [this](int folderCount) {
        progress_->setVisible(true);
        progress_->setRange(0, qMax(folderCount, 1));
        progress_->setValue(0);
        progress_->setFormat(tr("%1 / %2 个文件夹").arg(0).arg(folderCount));
        statusLabel_->setText(tr("正在扫描媒体库…"));
        updateScanControls();
    });
    connect(&library_, &MediaLibrary::scanProgress, this, [this](int processed, int total) {
        progress_->setRange(0, qMax(total, 1));
        progress_->setValue(processed);
        progress_->setFormat(tr("%1 / %2 个文件夹").arg(processed).arg(total));
    });
    connect(&library_, &MediaLibrary::scanFinished, this,
            [this](bool cancelled, const QString &error) {
                updateScanControls();
                if (cancelled) {
                    statusLabel_->setText(tr("扫描已取消。"));
                    return;
                }
                const auto count = library_.entries().size();
                if (!error.isEmpty())
                    statusLabel_->setText(tr("扫描完成，共 %1 项。\n%2").arg(count).arg(error));
                else
                    statusLabel_->setText(tr("扫描完成，共 %1 项。").arg(count));
                if (const auto saveError = library_.save(); !saveError.isEmpty())
                    statusLabel_->setText(saveError);
            });

    const auto loadError = library_.load();
    if (!loadError.isEmpty())
        statusLabel_->setText(loadError);
    rebuildFolders();
    rebuildEntries();
    updateScanControls();
}

void LibraryDialog::rebuildFolders() {
    const QSignalBlocker blocker(folderList_);
    const auto previous = folderList_->currentItem()
                              ? folderList_->currentItem()->data(Qt::UserRole).toString()
                              : QString{};
    folderList_->clear();
    for (const auto &folder : library_.folders()) {
        auto *item = new QListWidgetItem(folder, folderList_);
        item->setData(Qt::UserRole, folder);
        item->setToolTip(QDir::toNativeSeparators(folder));
        if (folder == previous)
            folderList_->setCurrentItem(item);
    }
    if (!folderList_->currentItem() && folderList_->count() > 0)
        folderList_->setCurrentRow(0);
    updateScanControls();
}

void LibraryDialog::rebuildEntries() {
    mediaModel_->removeRows(0, mediaModel_->rowCount());
    for (const auto &entry : library_.entries()) {
        auto *name = new QStandardItem(entry.displayName);
        name->setData(entry.path, Qt::UserRole);
        name->setEditable(false);
        name->setToolTip(QDir::toNativeSeparators(entry.path));
        const auto folder = QFileInfo(entry.path).absolutePath();
        auto *parent = new QStandardItem(QDir::toNativeSeparators(folder));
        parent->setEditable(false);
        parent->setToolTip(QDir::toNativeSeparators(folder));
        auto *path = new QStandardItem(QDir::toNativeSeparators(entry.path));
        path->setEditable(false);
        mediaModel_->appendRow({name, parent, path});
    }
    updateCount();
    updateEmptyState();
    updateSelectionActions();
}

void LibraryDialog::chooseFolder() {
    const auto start = lastDirectory_.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                           : lastDirectory_;
    const auto directory = QFileDialog::getExistingDirectory(this, tr("添加媒体文件夹"), start);
    if (directory.isEmpty())
        return;
    lastDirectory_ = directory;
    if (!library_.addFolder(directory)) {
        statusLabel_->setText(tr("无法添加文件夹：请选择可访问的绝对目录（最多 %1 个）。")
                                  .arg(MediaLibrary::MaximumFolders));
        return;
    }
    if (const auto error = library_.save(); !error.isEmpty())
        statusLabel_->setText(error);
    else
        statusLabel_->setText(tr("已添加文件夹，点击“刷新”开始扫描。"));
}

void LibraryDialog::removeSelectedFolder() {
    const auto *item = folderList_->currentItem();
    if (!item)
        return;
    if (!library_.removeFolder(item->data(Qt::UserRole).toString()))
        return;
    if (const auto error = library_.save(); !error.isEmpty())
        statusLabel_->setText(error);
    else
        statusLabel_->setText(tr("已移除文件夹。"));
}

void LibraryDialog::refresh() {
    if (library_.isScanning()) {
        library_.cancelScan();
        return;
    }
    statusLabel_->clear();
    library_.startScan();
}

void LibraryDialog::playSelected() {
    const auto path = currentPath();
    if (!path.isEmpty())
        emit playRequested(path);
}

void LibraryDialog::appendSelected() {
    const auto paths = selectedPaths();
    if (!paths.isEmpty())
        emit appendRequested(paths);
}

void LibraryDialog::updateCount() {
    const auto total = mediaModel_->rowCount();
    const auto visible = filter_->rowCount();
    if (search_->text().trimmed().isEmpty())
        countLabel_->setText(tr("共 %1 项").arg(total));
    else
        countLabel_->setText(tr("显示 %1 / 共 %2 项").arg(visible).arg(total));
}

void LibraryDialog::updateEmptyState() {
    const bool hasEntries = mediaModel_->rowCount() > 0;
    const bool hasVisible = filter_->rowCount() > 0;
    if (!hasEntries) {
        if (library_.isScanning()) {
            emptyLabel_->hide();
            return;
        }
        if (library_.folders().isEmpty())
            emptyLabel_->setText(tr("尚未添加媒体文件夹。\n点击“添加文件夹…”并刷新以建立媒体库。"));
        else
            emptyLabel_->setText(tr("媒体库为空。\n点击“刷新”扫描已添加的文件夹。"));
        emptyLabel_->show();
        return;
    }
    if (!hasVisible) {
        emptyLabel_->setText(tr("没有匹配的媒体。"));
        emptyLabel_->show();
        return;
    }
    emptyLabel_->hide();
}

void LibraryDialog::updateSelectionActions() {
    const bool hasSelection = !selectedPaths().isEmpty();
    playButton_->setEnabled(hasSelection);
    appendButton_->setEnabled(hasSelection);
}

void LibraryDialog::updateScanControls() {
    const bool scanning = library_.isScanning();
    refreshButton_->setText(scanning ? tr("取消") : tr("刷新"));
    refreshButton_->setAccessibleName(scanning ? tr("取消媒体库扫描") : tr("刷新媒体库"));
    progress_->setVisible(scanning);
    folderList_->setEnabled(!scanning);
    addFolderButton_->setEnabled(!scanning);
    removeFolderButton_->setEnabled(!scanning && folderList_->currentItem() != nullptr);
    recursiveCheck_->setEnabled(!scanning);
    updateEmptyState();
}

QString LibraryDialog::currentPath() const {
    const auto index = mediaView_->currentIndex();
    if (!index.isValid())
        return {};
    const auto source = filter_->mapToSource(index);
    const auto *item = mediaModel_->item(source.row(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

QStringList LibraryDialog::selectedPaths() const {
    QStringList paths;
    const auto *selection = mediaView_->selectionModel();
    if (!selection)
        return paths;
    const auto rows = selection->selectedRows(0);
    for (const auto &index : rows) {
        const auto source = filter_->mapToSource(index);
        if (const auto *item = mediaModel_->item(source.row(), 0)) {
            const auto path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty())
                paths.append(path);
        }
    }
    return paths;
}

namespace LibraryDialogSupport {
LibraryDialog *show(QWidget *host, std::function<void(const QString &)> playRequested,
                    std::function<void(const QStringList &)> appendRequested) {
    if (!host)
        return nullptr;
    auto *library = host->findChild<MediaLibrary *>(QStringLiteral("mediaLibrary"));
    if (!library) {
        const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
        library = new MediaLibrary(QDir(directory).filePath(QStringLiteral("library.ini")), host);
        library->setObjectName(QStringLiteral("mediaLibrary"));
    }
    auto *dialog = host->findChild<LibraryDialog *>();
    if (!dialog) {
        dialog = new LibraryDialog(*library, host);
        if (playRequested) {
            QObject::connect(dialog, &LibraryDialog::playRequested, host,
                             [playRequested = std::move(playRequested)](const QString &path) {
                                 playRequested(path);
                             });
        }
        if (appendRequested) {
            QObject::connect(dialog, &LibraryDialog::appendRequested, host,
                             [appendRequested = std::move(appendRequested)](
                                 const QStringList &paths) { appendRequested(paths); });
        }
    }
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    return dialog;
}
} // namespace LibraryDialogSupport
