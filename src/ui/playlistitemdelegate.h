#pragma once

#include <QStyledItemDelegate>

class PlaylistItemDelegate final : public QStyledItemDelegate {
    Q_OBJECT
  public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const override;
};
