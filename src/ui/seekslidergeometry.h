#pragma once

#include <QRect>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QtGlobal>

namespace SeekSliderGeometry {

inline int valueAtPosition(const QStyleOptionSlider &option, const QRect &groove,
                           const QSize &handle, const QPoint &position) {
    const bool horizontal = option.orientation == Qt::Horizontal;
    const int start =
        horizontal ? groove.left() + handle.width() / 2 : groove.top() + handle.height() / 2;
    const int extent =
        horizontal ? groove.width() - handle.width() : groove.height() - handle.height();
    const int coordinate = horizontal ? position.x() : position.y();
    return QStyle::sliderValueFromPosition(option.minimum, option.maximum, coordinate - start,
                                           qMax(0, extent), option.upsideDown);
}

} // namespace SeekSliderGeometry
