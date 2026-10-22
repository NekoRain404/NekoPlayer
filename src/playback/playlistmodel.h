#pragma once

#include <QAbstractListModel>
#include <QStringList>

class PlaylistModel final : public QAbstractListModel {
    Q_OBJECT
  public:
    enum Roles { PathRole = Qt::UserRole + 1, CurrentRole };

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
    [[nodiscard]] QStringList paths() const;

    int addFiles(const QStringList &paths);
    bool removeAt(int row);
    void clear();
    bool setCurrentIndex(int row);

  signals:
    void currentIndexChanged(int row);

  private:
    QStringList paths_;
    int currentIndex_ = -1;
};
