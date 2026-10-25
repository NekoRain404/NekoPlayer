#include "playlistmaintenancemenu.h"
#include "playback/playlistcontroller.h"

#include <QAction>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QWidget>
#include <array>
#include <utility>

namespace {
class WaitCursorGuard final {
  public:
    WaitCursorGuard() { QGuiApplication::setOverrideCursor(Qt::WaitCursor); }
    ~WaitCursorGuard() { QGuiApplication::restoreOverrideCursor(); }
    Q_DISABLE_COPY(WaitCursorGuard)
};
} // namespace

PlaylistMaintenanceMenu::PlaylistMaintenanceMenu(PlaylistController &playlist,
                                                 QWidget *dialogParent, QObject *parent)
    : QObject(parent), playlist_(playlist), dialogParent_(dialogParent),
      sortMenu_(new QMenu(dialogParent)) {
    sortMenu_->setObjectName(QStringLiteral("playlistSortMenu"));
    using SortKey = PlaylistModel::SortKey;
    const auto sortEntries = std::array{
        std::pair{SortKey::Title, Qt::AscendingOrder},
        std::pair{SortKey::Title, Qt::DescendingOrder},
        std::pair{SortKey::Path, Qt::AscendingOrder},
        std::pair{SortKey::Path, Qt::DescendingOrder},
    };
    const auto names = std::array{
        "sortByTitleAscendingAction",
        "sortByTitleDescendingAction",
        "sortByPathAscendingAction",
        "sortByPathDescendingAction",
    };
    for (size_t index = 0; index < sortEntries.size(); ++index) {
        auto *action = sortMenu_->addAction(QString{});
        action->setObjectName(QString::fromLatin1(names[index]));
        sortActions_[index] = action;
        const auto [key, order] = sortEntries[index];
        connect(action, &QAction::triggered, this, [this, key, order] {
            const WaitCursorGuard waitCursor;
            playlist_.sortPlaylist(key, order);
        });
    }
    removeMissingAction_ = new QAction(this);
    removeMissingAction_->setObjectName(QStringLiteral("removeMissingFilesAction"));
    connect(removeMissingAction_, &QAction::triggered, this,
            &PlaylistMaintenanceMenu::removeMissingFiles);
    retranslate();
}

QMenu *PlaylistMaintenanceMenu::sortMenu() const { return sortMenu_; }

QAction *PlaylistMaintenanceMenu::removeMissingAction() const { return removeMissingAction_; }

void PlaylistMaintenanceMenu::removeMissingFiles() {
    QList<int> rows;
    {
        const WaitCursorGuard waitCursor;
        rows = playlist_.missingFileRows();
    }
    if (rows.isEmpty()) {
        emit feedback(tr("没有缺失文件"));
        return;
    }
    const auto answer =
        QMessageBox::question(dialogParent_, tr("移除缺失文件"),
                              tr("找到 %1 个缺失文件。要从播放列表中移除吗？").arg(rows.size()),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;
    int removed = 0;
    {
        const WaitCursorGuard waitCursor;
        // The modal dialog ran a nested event loop; a folder import may have
        // replaced the queue, so revalidate instead of reusing row numbers.
        removed = playlist_.removeMissingFiles();
    }
    emit feedback(removed == 0 ? tr("没有缺失文件") : tr("已移除 %1 个缺失文件").arg(removed));
}

void PlaylistMaintenanceMenu::retranslate() {
    sortMenu_->setTitle(tr("排序"));
    const auto labels = std::array{tr("标题升序"), tr("标题降序"), tr("路径升序"), tr("路径降序")};
    for (size_t index = 0; index < labels.size(); ++index)
        sortActions_[index]->setText(labels[index]);
    removeMissingAction_->setText(tr("移除缺失文件…"));
}
