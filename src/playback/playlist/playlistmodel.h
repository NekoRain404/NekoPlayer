#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QStringList>

struct PlaylistEntryMetadata {
    double durationSeconds = -1; // -1 means unknown, matching #EXTINF:-1
    QString title;               // optional display title from the playlist
    bool operator==(const PlaylistEntryMetadata &) const = default;
};

class PlaylistModel final : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles { PathRole = Qt::UserRole + 1, CurrentRole };
    enum class SortKey { Title, Path };

    explicit PlaylistModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index,
                                int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;
    [[nodiscard]] Qt::DropActions supportedDropActions() const override;
    [[nodiscard]] QStringList mimeTypes() const override;
    [[nodiscard]] QMimeData *mimeData(const QModelIndexList &indexes) const override;
    [[nodiscard]] bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                                       int column, const QModelIndex &parent) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;
    [[nodiscard]] int currentIndex() const;
    [[nodiscard]] QString currentPath() const;
    [[nodiscard]] QString pathAt(int row) const;
    [[nodiscard]] const QStringList &paths() const;

    int addFiles(const QStringList &paths, const QList<PlaylistEntryMetadata> &metadata = {});
    [[nodiscard]] PlaylistEntryMetadata metadataAt(int row) const;
    [[nodiscard]] QList<PlaylistEntryMetadata> metadata() const;
    int removeRowsAt(const QList<int> &rows);
    bool removeAt(int row);
    bool sortEntries(SortKey key, Qt::SortOrder order);
    void clear();
    bool setCurrentIndex(int row);

  signals:
    void currentIndexChanged(int row);

  private:
    QStringList paths_;
    QStringList displayNames_;
    // Row-aligned optional playlist metadata. It follows inserts, moves,
    // sorting and removal so a re-saved playlist keeps its #EXTINF lines.
    QList<PlaylistEntryMetadata> metadata_;
    int currentIndex_ = -1;
};
