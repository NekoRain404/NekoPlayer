#include "thumbnailpreview.h"

#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <algorithm>

namespace {
QString timestampText(double seconds) {
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600, 2, 10, QLatin1Char('0'))
        .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

ThumbnailPreview::ThumbnailPreview(QWidget *parent)
    : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
    setObjectName(QStringLiteral("thumbnailPreview"));
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(false);
}

void ThumbnailPreview::present(const QImage &image, double seconds) {
    image_ = image;
    seconds_ = seconds;
    setFixedSize(sizeHint());
    update();
}

void ThumbnailPreview::clear() {
    image_ = {};
    hide();
    update();
}

QSize ThumbnailPreview::sizeHint() const { return QSize(248, 166); }

void ThumbnailPreview::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const auto frame = rect().adjusted(1, 1, -1, -1);
    const auto panel = palette().color(QPalette::Window);
    painter.setPen(QPen(palette().color(QPalette::Mid), 1));
    painter.setBrush(panel);
    painter.drawRoundedRect(frame, 10, 10);

    const auto imageRect = frame.adjusted(8, 8, -8, -28);
    if (image_.isNull()) {
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.drawText(imageRect, Qt::AlignCenter, tr("预览不可用"));
    } else {
        painter.setClipRect(imageRect);
        painter.drawImage(imageRect, image_, image_.rect());
        painter.setClipping(false);
    }

    const auto badge = QRect(frame.left() + 8, frame.bottom() - 25, frame.width() - 16, 17);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 165));
    painter.drawRoundedRect(badge, 5, 5);
    painter.setPen(Qt::white);
    painter.drawText(badge, Qt::AlignCenter, timestampText(seconds_));
}
