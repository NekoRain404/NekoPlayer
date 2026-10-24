#include "playlistitemdelegate.h"

#include "playback/playlistmodel.h"

void PlaylistItemDelegate::initStyleOption(QStyleOptionViewItem *option,
                                           const QModelIndex &index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    if (!index.data(PlaylistModel::CurrentRole).toBool())
        return;
    option->font.setBold(true);
    option->text.prepend(QStringLiteral("▶ "));
}
