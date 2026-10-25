#include "playlistmodel.h"
#include "mediasource.h"

#include <QFileInfo>
#include <QMimeData>
#include <QPersistentModelIndex>
#include <QUrl>

namespace {
constexpr auto playlistMimeType = "application/x-nekoplayer-playlist-entry";

class PlaylistDragData final : public QMimeData {
  public:
    explicit PlaylistDragData(const QModelIndex &index) : entry(index) {
        setData(QString::fromLatin1(playlistMimeType), "entry");
    }
    QPersistentModelIndex entry;
};
} // namespace

PlaylistModel::PlaylistModel(QObject *parent) : QAbstractListModel(parent) {}

int PlaylistModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : paths_.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.model() != this || index.row() < 0 ||
        index.row() >= paths_.size())
        return {};
    const auto &path = paths_.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return displayNames_.value(index.row());
    case PathRole:
    case Qt::ToolTipRole:
        return path;
    case CurrentRole:
        return index.row() == currentIndex_;
    default:
        return {};
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return index.model() == this ? Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled
                                 : Qt::NoItemFlags;
}

Qt::DropActions PlaylistModel::supportedDropActions() const { return Qt::MoveAction; }

QStringList PlaylistModel::mimeTypes() const { return {QString::fromLatin1(playlistMimeType)}; }

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const {
    if (indexes.size() != 1 || !indexes.first().isValid() || indexes.first().model() != this)
        return nullptr;
    // A live index follows edits during the drag and distinguishes duplicate paths.
    return new PlaylistDragData(indexes.first());
}

bool PlaylistModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                                    int column, const QModelIndex &parent) const {
    const auto *drag = dynamic_cast<const PlaylistDragData *>(data);
    return action == Qt::MoveAction && !parent.isValid() && column >= -1 && column <= 0 &&
           row >= -1 && row <= rowCount() && drag && drag->entry.isValid() &&
           drag->entry.model() == this;
}

bool PlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                                 const QModelIndex &parent) {
    if (!canDropMimeData(data, action, row, column, parent))
        return false;
    const auto *drag = static_cast<const PlaylistDragData *>(data);
    return moveRow({}, drag->entry.row(), {}, row == -1 ? rowCount() : row);
}

bool PlaylistModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                             const QModelIndex &destinationParent, int destinationChild) {
    if (sourceParent.isValid() || destinationParent.isValid() || count != 1 || sourceRow < 0 ||
        sourceRow >= rowCount() || destinationChild < 0 || destinationChild > rowCount() ||
        destinationChild == sourceRow || destinationChild == sourceRow + 1)
        return false;
    if (!beginMoveRows({}, sourceRow, sourceRow, {}, destinationChild))
        return false;
    const auto target = destinationChild > sourceRow ? destinationChild - 1 : destinationChild;
    const auto previous = currentIndex_;
    paths_.move(sourceRow, target);
    displayNames_.move(sourceRow, target);
    if (currentIndex_ == sourceRow)
        currentIndex_ = target;
    else if (sourceRow < currentIndex_ && currentIndex_ <= target)
        --currentIndex_;
    else if (target <= currentIndex_ && currentIndex_ < sourceRow)
        ++currentIndex_;
    endMoveRows();
    if (currentIndex_ != previous)
        emit currentIndexChanged(currentIndex_);
    return true;
}

int PlaylistModel::currentIndex() const { return currentIndex_; }

QString PlaylistModel::currentPath() const {
    return currentIndex_ >= 0 ? paths_.value(currentIndex_) : QString{};
}

QString PlaylistModel::pathAt(int row) const { return paths_.value(row); }
const QStringList &PlaylistModel::paths() const { return paths_; }

int PlaylistModel::addFiles(const QStringList &paths) {
    QStringList additions;
    QStringList displayNames;
    for (const auto &path : paths) {
        const QUrl url(path, QUrl::StrictMode);
        const auto absolute =
            url.isValid() && !url.scheme().isEmpty() ? path : QFileInfo(path).absoluteFilePath();
        if (!absolute.isEmpty()) {
            additions.append(absolute);
            displayNames.append(MediaSource::displayName(absolute));
        }
    }
    if (additions.isEmpty())
        return 0;
    const auto first = paths_.size();
    beginInsertRows({}, first, first + additions.size() - 1);
    paths_.append(additions);
    displayNames_.append(displayNames);
    endInsertRows();
    return additions.size();
}

bool PlaylistModel::removeAt(int row) {
    if (row < 0 || row >= paths_.size())
        return false;
    const auto previousCurrent = currentIndex_;
    beginRemoveRows({}, row, row);
    paths_.removeAt(row);
    displayNames_.removeAt(row);
    if (currentIndex_ > row)
        --currentIndex_;
    else if (currentIndex_ == row)
        currentIndex_ = paths_.isEmpty() ? -1 : qMin(row, paths_.size() - 1);
    endRemoveRows();
    if (currentIndex_ >= 0)
        emit dataChanged(index(currentIndex_), index(currentIndex_), {CurrentRole});
    if (currentIndex_ != previousCurrent || previousCurrent == row)
        emit currentIndexChanged(currentIndex_);
    return true;
}

void PlaylistModel::clear() {
    if (paths_.isEmpty())
        return;
    beginResetModel();
    paths_.clear();
    displayNames_.clear();
    const bool hadCurrent = currentIndex_ != -1;
    currentIndex_ = -1;
    endResetModel();
    if (hadCurrent)
        emit currentIndexChanged(-1);
}

bool PlaylistModel::setCurrentIndex(int row) {
    if (row < 0 || row >= paths_.size())
        return false;
    if (row == currentIndex_)
        return true;
    const auto previous = currentIndex_;
    currentIndex_ = row;
    if (previous >= 0)
        emit dataChanged(index(previous), index(previous), {CurrentRole});
    emit dataChanged(index(row), index(row), {CurrentRole});
    emit currentIndexChanged(row);
    return true;
}
