#include "appicon.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <array>

namespace {
constexpr int kDesignSize = 256;
constexpr std::array<int, 7> kIconSizes = {16, 24, 32, 48, 64, 128, 256};
const QColor kTileColor(0x1f, 0x22, 0x28);
const QColor kAccentColor(0x8b, 0xa9, 0xef);

// Paths use the 256x256 coordinate space of data/nekoplayer.svg.
QPainterPath tilePath() {
    QPainterPath path;
    path.moveTo(24, 88);
    path.lineTo(24, 78);
    path.quadTo(24, 72, 28, 66);
    path.lineTo(48, 22);
    path.quadTo(52, 12, 61, 19);
    path.lineTo(104, 56);
    path.lineTo(152, 56);
    path.lineTo(195, 19);
    path.quadTo(204, 12, 208, 22);
    path.lineTo(228, 66);
    path.quadTo(232, 72, 232, 78);
    path.lineTo(232, 188);
    path.quadTo(232, 232, 188, 232);
    path.lineTo(68, 232);
    path.quadTo(24, 232, 24, 188);
    path.closeSubpath();
    return path;
}

QPainterPath playPath() {
    QPainterPath path;
    path.moveTo(108, 106);
    path.quadTo(108, 100, 114, 104);
    path.lineTo(174, 140);
    path.quadTo(180, 144, 174, 148);
    path.lineTo(114, 184);
    path.quadTo(108, 188, 108, 182);
    path.closeSubpath();
    return path;
}

QPixmap renderIcon(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal scale = static_cast<qreal>(size) / kDesignSize;
    painter.scale(scale, scale);

    QPen outline(kAccentColor);
    outline.setJoinStyle(Qt::RoundJoin);
    // Keep the outline at least one device pixel wide at small sizes.
    outline.setWidthF(qMax(8.0, kDesignSize / static_cast<qreal>(size)));
    painter.setPen(outline);
    painter.setBrush(kTileColor);
    painter.drawPath(tilePath());

    painter.setPen(Qt::NoPen);
    painter.setBrush(kAccentColor);
    painter.drawPath(playPath());
    return pixmap;
}
} // namespace

QIcon AppIcon::windowIcon() {
    QIcon icon;
    for (const int size : kIconSizes)
        icon.addPixmap(renderIcon(size));
    return icon;
}
