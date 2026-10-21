#include "playlistmodel.h"

#include <QFileInfo>

PlaylistModel::PlaylistModel(QObject *parent) : QAbstractListModel(parent) {}

int PlaylistModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : paths_.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= paths_.size())
        return {};
    const auto &path = paths_.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QFileInfo(path).fileName();
    case PathRole:
        return path;
    case CurrentRole:
        return index.row() == currentIndex_;
    default:
        return {};
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const {
    return index.isValid() ? Qt::ItemIsEnabled | Qt::ItemIsSelectable : Qt::NoItemFlags;
}

int PlaylistModel::currentIndex() const { return currentIndex_; }

QString PlaylistModel::currentPath() const {
    return currentIndex_ >= 0 ? paths_.value(currentIndex_) : QString{};
}

QString PlaylistModel::pathAt(int row) const { return paths_.value(row); }

int PlaylistModel::addFiles(const QStringList &paths) {
    QStringList additions;
    for (const auto &path : paths) {
        const auto absolute = QFileInfo(path).absoluteFilePath();
        if (!absolute.isEmpty())
            additions.append(absolute);
    }
    if (additions.isEmpty())
        return 0;
    const auto first = paths_.size();
    beginInsertRows({}, first, first + additions.size() - 1);
    paths_.append(additions);
    endInsertRows();
    return additions.size();
}

bool PlaylistModel::removeAt(int row) {
    if (row < 0 || row >= paths_.size())
        return false;
    const auto previousCurrent = currentIndex_;
    beginRemoveRows({}, row, row);
    paths_.removeAt(row);
    if (currentIndex_ > row)
        --currentIndex_;
    else if (currentIndex_ == row)
        currentIndex_ = paths_.isEmpty() ? -1 : qMin(row, paths_.size() - 1);
    endRemoveRows();
    if (currentIndex_ != previousCurrent)
        emit currentIndexChanged(currentIndex_);
    return true;
}

void PlaylistModel::clear() {
    if (paths_.isEmpty())
        return;
    beginResetModel();
    paths_.clear();
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
