#include "appearanceicons.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

namespace {
using AppearanceIcons::Symbol;

void paintSymbol(QPainter &painter, Symbol symbol) {
    const auto line = [&painter](qreal x1, qreal y1, qreal x2, qreal y2) {
        painter.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    };
    const auto polygon = [&painter](std::initializer_list<QPointF> points) {
        QPolygonF shape;
        for (const auto &point : points)
            shape << point;
        painter.drawPolygon(shape);
    };
    switch (symbol) {
    case Symbol::Play:
        polygon({{7, 4}, {19, 12}, {7, 20}});
        break;
    case Symbol::Pause:
        painter.drawRoundedRect(QRectF(6, 4, 4, 16), 1, 1);
        painter.drawRoundedRect(QRectF(14, 4, 4, 16), 1, 1);
        break;
    case Symbol::Stop:
        painter.drawRoundedRect(QRectF(5, 5, 14, 14), 1, 1);
        break;
    case Symbol::Previous:
        line(5, 5, 5, 19);
        polygon({{18, 5}, {9, 12}, {18, 19}});
        break;
    case Symbol::Next:
        line(19, 5, 19, 19);
        polygon({{6, 5}, {15, 12}, {6, 19}});
        break;
    case Symbol::Seek:
        line(5, 5, 5, 19);
        polygon({{10, 5}, {18, 12}, {10, 19}});
        line(19, 5, 19, 19);
        break;
    case Symbol::Open:
    case Symbol::MediaFile:
        painter.drawRoundedRect(QRectF(5, 3, 11, 18), 1, 1);
        line(8, 8, 13, 8);
        line(8, 12, 13, 12);
        line(8, 16, 13, 16);
        if (symbol == Symbol::Open) {
            line(14, 17, 19, 17);
            line(16.5, 14.5, 19, 17);
            line(16.5, 19.5, 19, 17);
        }
        break;
    case Symbol::Network:
        painter.drawRoundedRect(QRectF(3, 4, 18, 12), 2, 2);
        line(12, 16, 12, 20);
        line(8, 20, 16, 20);
        painter.drawEllipse(QPointF(7, 10), 1, 1);
        painter.drawEllipse(QPointF(12, 10), 1, 1);
        painter.drawEllipse(QPointF(17, 10), 1, 1);
        break;
    case Symbol::Playlist:
        line(4, 6, 13, 6);
        line(4, 11, 13, 11);
        line(4, 16, 13, 16);
        polygon({{16, 7}, {21, 10}, {16, 13}});
        break;
    case Symbol::Save:
        painter.drawRoundedRect(QRectF(4, 3, 16, 18), 1, 1);
        painter.drawRect(QRectF(7, 3, 9, 6));
        painter.drawRect(QRectF(7, 14, 10, 7));
        break;
    case Symbol::Add:
        painter.drawEllipse(QRectF(3, 3, 18, 18));
        line(12, 7, 12, 17);
        line(7, 12, 17, 12);
        break;
    case Symbol::Fullscreen:
    case Symbol::Restore:
        line(4, 9, 4, 4);
        line(4, 4, 9, 4);
        line(15, 4, 20, 4);
        line(20, 4, 20, 9);
        line(4, 15, 4, 20);
        line(4, 20, 9, 20);
        line(15, 20, 20, 20);
        line(20, 15, 20, 20);
        if (symbol == Symbol::Restore)
            painter.drawRoundedRect(QRectF(8, 8, 8, 8), 1, 1);
        break;
    case Symbol::Volume:
    case Symbol::Mute:
        polygon({{3, 9}, {7, 9}, {12, 5}, {12, 19}, {7, 15}, {3, 15}});
        if (symbol == Symbol::Volume) {
            painter.drawArc(QRectF(10, 6, 8, 12), -60 * 16, 120 * 16);
            painter.drawArc(QRectF(10, 3, 14, 18), -60 * 16, 120 * 16);
        } else {
            line(15, 9, 21, 15);
            line(21, 9, 15, 15);
        }
        break;
    case Symbol::Remove:
        painter.drawEllipse(QRectF(3, 3, 18, 18));
        line(7, 12, 17, 12);
        break;
    case Symbol::MoveUp:
    case Symbol::MoveDown:
        line(12, 4, 12, 20);
        if (symbol == Symbol::MoveUp) {
            line(6, 10, 12, 4);
            line(18, 10, 12, 4);
        } else {
            line(6, 14, 12, 20);
            line(18, 14, 12, 20);
        }
        break;
    case Symbol::Information:
        painter.drawEllipse(QRectF(3, 3, 18, 18));
        painter.drawEllipse(QPointF(12, 8), 0.8, 0.8);
        line(12, 11, 12, 17);
        break;
    }
}

class AppearanceIconEngine final : public QIconEngine {
  public:
    AppearanceIconEngine(Symbol symbol, QPalette palette) : symbol_(symbol), palette_(palette) {}
    QIconEngine *clone() const override { return new AppearanceIconEngine(*this); }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State) override {
        const auto group = mode == QIcon::Disabled ? QPalette::Disabled : QPalette::Active;
        const auto color = palette_.color(group, QPalette::ButtonText);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->translate(rect.topLeft());
        painter->scale(rect.width() / 24.0, rect.height() / 24.0);
        painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(color);
        paintSymbol(*painter, symbol_);
        painter->restore();
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state,
                         qreal scale) override {
        QPixmap pixmap(size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        paint(&painter, QRect(QPoint(0, 0), size), mode, state);
        return pixmap;
    }

  private:
    Symbol symbol_;
    QPalette palette_;
};
} // namespace

QIcon AppearanceIcons::icon(Symbol symbol, const QPalette &palette) {
    return QIcon(new AppearanceIconEngine(symbol, palette));
}
