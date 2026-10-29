#include "playback/playlist/playlistmodel.h"
#include "playback/media/mediasource.h"
#include "playback/playlist/naturalcompare.h"

#include <QFileInfo>
#include <QMimeData>
#include <QPersistentModelIndex>
#include <QUrl>

#include <algorithm>
#include <numeric>
#include <vector>

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
        Q_ASSERT(displayNames_.size() == paths_.size());
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
    metadata_.move(sourceRow, target);
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

namespace {
QString metadataDisplayName(const QString &path, const PlaylistEntryMetadata &metadata) {
    auto title = metadata.title;
    for (auto &character : title) {
        const auto category = character.category();
        if (category == QChar::Other_Control || category == QChar::Other_Format ||
            category == QChar::Separator_Line || category == QChar::Separator_Paragraph)
            character = QLatin1Char(' ');
    }
    title = title.simplified();
    if (title.size() > 256)
        title = title.left(255) + QChar(0x2026);
    return title.isEmpty() ? MediaSource::displayName(path) : title;
}
} // namespace

int PlaylistModel::addFiles(const QStringList &paths,
                            const QList<PlaylistEntryMetadata> &metadata) {
    const bool aligned = metadata.size() == paths.size();
    QStringList additions;
    QStringList displayNames;
    QList<PlaylistEntryMetadata> additionMetadata;
    for (qsizetype index = 0; index < paths.size(); ++index) {
        const auto &path = paths.at(index);
        const QUrl url(path, QUrl::StrictMode);
        const auto absolute =
            url.isValid() && !url.scheme().isEmpty() ? path : QFileInfo(path).absoluteFilePath();
        if (!absolute.isEmpty()) {
            const auto entry = aligned ? metadata.at(index) : PlaylistEntryMetadata{};
            additions.append(absolute);
            displayNames.append(metadataDisplayName(absolute, entry));
            additionMetadata.append(entry);
        }
    }
    if (additions.isEmpty())
        return 0;
    const auto first = paths_.size();
    beginInsertRows({}, first, first + additions.size() - 1);
    paths_.append(additions);
    displayNames_.append(displayNames);
    metadata_.append(additionMetadata);
    endInsertRows();
    return additions.size();
}

PlaylistEntryMetadata PlaylistModel::metadataAt(int row) const {
    return row >= 0 && row < metadata_.size() ? metadata_.at(row) : PlaylistEntryMetadata{};
}

QList<PlaylistEntryMetadata> PlaylistModel::metadata() const { return metadata_; }

int PlaylistModel::removeRowsAt(const QList<int> &rows) {
    std::vector<int> removed;
    removed.reserve(rows.size());
    for (const auto row : rows) {
        if (row >= 0 && row < paths_.size())
            removed.push_back(row);
    }
    std::sort(removed.begin(), removed.end());
    removed.erase(std::unique(removed.begin(), removed.end()), removed.end());
    if (removed.empty())
        return 0;

    const auto isRemoved = [&removed](int row) {
        return std::binary_search(removed.cbegin(), removed.cend(), row);
    };
    const auto removedBefore = [&removed](int row) {
        return static_cast<int>(std::lower_bound(removed.cbegin(), removed.cend(), row) -
                                removed.cbegin());
    };

    const int previousCurrent = currentIndex_;
    const bool currentRemoved = previousCurrent >= 0 && isRemoved(previousCurrent);
    int finalCurrent = -1;
    if (previousCurrent >= 0) {
        if (!currentRemoved) {
            // The current entry survives; only rows above it shift its row.
            finalCurrent = previousCurrent - removedBefore(previousCurrent);
        } else {
            // Prefer the next surviving original row, then the previous one.
            auto replacement = previousCurrent + 1;
            while (replacement < paths_.size() && isRemoved(replacement))
                ++replacement;
            if (replacement == paths_.size()) {
                replacement = previousCurrent - 1;
                while (replacement >= 0 && isRemoved(replacement))
                    --replacement;
            }
            if (replacement >= 0)
                finalCurrent = replacement - removedBefore(replacement);
        }
    }

    struct RemovalRange {
        int first;
        int last;
    };
    std::vector<RemovalRange> ranges;
    ranges.reserve(removed.size());
    for (const auto row : removed) {
        if (!ranges.empty() && row == ranges.back().last + 1)
            ranges.back().last = row;
        else
            ranges.push_back({row, row});
    }

    // Descending ranges keep every intermediate row number valid, and a run of
    // adjacent rows costs one notification instead of one notification per row.
    for (auto it = ranges.crbegin(); it != ranges.crend(); ++it) {
        const auto first = it->first;
        const auto last = it->last;
        const auto count = last - first + 1;
        beginRemoveRows({}, first, last);
        paths_.remove(first, count);
        displayNames_.remove(first, count);
        metadata_.remove(first, count);
        if (currentIndex_ >= first && currentIndex_ <= last)
            currentIndex_ = -1;
        else if (currentIndex_ > last)
            currentIndex_ -= count;
        endRemoveRows();
    }
    currentIndex_ = finalCurrent;
    if (currentIndex_ >= 0)
        emit dataChanged(index(currentIndex_), index(currentIndex_), {CurrentRole});
    if (finalCurrent != previousCurrent || currentRemoved)
        emit currentIndexChanged(finalCurrent);
    return static_cast<int>(removed.size());
}

bool PlaylistModel::removeAt(int row) { return removeRowsAt({row}) == 1; }

bool PlaylistModel::sortEntries(SortKey key, Qt::SortOrder order) {
    const auto count = paths_.size();
    if (count < 2)
        return false;
    // Collation dominates the sort cost, so build one key per row. Every
    // comparison below then only compares precomputed keys; for a given collator
    // QCollatorSortKey::compare orders like QCollator::compare, which keeps the
    // observable order identical.
    const auto &sortTexts = key == SortKey::Title ? displayNames_ : paths_;
    std::vector<QCollatorSortKey> sortKeys;
    sortKeys.reserve(static_cast<std::size_t>(count));
    for (const auto &text : sortTexts)
        sortKeys.push_back(naturalSortKey(text));

    std::vector<int> permutation(static_cast<std::size_t>(count));
    std::iota(permutation.begin(), permutation.end(), 0);
    const auto ascending = order == Qt::AscendingOrder;
    const auto compareRows = [this, key, &sortKeys](int left, int right) {
        auto compared = sortKeys[static_cast<std::size_t>(left)].compare(
            sortKeys[static_cast<std::size_t>(right)]);
        if (compared == 0 && key == SortKey::Title)
            compared = QString::compare(paths_.at(left), paths_.at(right), Qt::CaseSensitive);
        return compared;
    };
    // Stable sorting keeps duplicates in their original order; the comparator
    // itself only considers the requested key and its deterministic tiebreak.
    std::stable_sort(permutation.begin(), permutation.end(),
                     [&compareRows, ascending](int left, int right) {
                         const auto compared = compareRows(left, right);
                         return ascending ? compared < 0 : compared > 0;
                     });
    auto unchanged = true;
    for (auto row = 0; row < count; ++row) {
        if (permutation[static_cast<std::size_t>(row)] != row) {
            unchanged = false;
            break;
        }
    }
    if (unchanged)
        return false;

    std::vector<int> newRows(static_cast<std::size_t>(count));
    for (auto row = 0; row < count; ++row) {
        newRows[static_cast<std::size_t>(permutation[static_cast<std::size_t>(row)])] = row;
    }
    const int previousCurrent = currentIndex_;
    const int newCurrent =
        previousCurrent >= 0 ? newRows[static_cast<std::size_t>(previousCurrent)] : -1;
    emit layoutAboutToBeChanged({}, QAbstractItemModel::VerticalSortHint);
    // Views and the model tester create persistent indexes in response to the
    // signal above; capture them afterwards so every one is remapped exactly
    // once and no two entries end up sharing a row.
    const auto persistent = persistentIndexList();
    QModelIndexList from;
    QModelIndexList to;
    from.reserve(persistent.size());
    to.reserve(persistent.size());
    for (const auto &index : persistent) {
        if (!index.isValid())
            continue;
        from.append(index);
        to.append(createIndex(newRows[static_cast<std::size_t>(index.row())], index.column()));
    }
    if (!from.isEmpty())
        changePersistentIndexList(from, to);
    QStringList sortedPaths;
    QStringList sortedNames;
    QList<PlaylistEntryMetadata> sortedMetadata;
    sortedPaths.reserve(count);
    sortedNames.reserve(count);
    sortedMetadata.reserve(count);
    for (const auto source : permutation) {
        sortedPaths.append(paths_.at(source));
        sortedNames.append(displayNames_.at(source));
        sortedMetadata.append(metadata_.value(source));
    }
    paths_ = std::move(sortedPaths);
    displayNames_ = std::move(sortedNames);
    metadata_ = std::move(sortedMetadata);
    currentIndex_ = newCurrent;
    emit layoutChanged({}, QAbstractItemModel::VerticalSortHint);

    // Refresh the delegate at the old and new current rows; the entry itself
    // stays current, only its row (and therefore its CurrentRole) may move.
    if (previousCurrent >= 0 && previousCurrent != currentIndex_)
        emit dataChanged(index(previousCurrent), index(previousCurrent), {CurrentRole});
    if (currentIndex_ >= 0)
        emit dataChanged(index(currentIndex_), index(currentIndex_), {CurrentRole});
    if (currentIndex_ != previousCurrent)
        emit currentIndexChanged(currentIndex_);
    return true;
}

void PlaylistModel::clear() {
    if (paths_.isEmpty())
        return;
    beginResetModel();
    paths_.clear();
    displayNames_.clear();
    metadata_.clear();
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
