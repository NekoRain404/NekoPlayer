#pragma once

#include <QRect>
#include <QSize>

namespace ActionFeedbackGeometry {
struct Layout {
    QRect panel;
    QRect text;
};

inline int boundedExtent(int requested, int available) {
    if (available <= 0)
        return 0;
    return qBound(1, requested, available);
}

inline Layout arrange(const QSize &viewport, const QSize &textHint) {
    const int viewportWidth = qMax(0, viewport.width());
    const int viewportHeight = qMax(0, viewport.height());
    const int panelWidth = boundedExtent(textHint.width() + 36, viewportWidth);
    const int panelHeight = boundedExtent(textHint.height() + 20, viewportHeight);
    const int desiredY = qRound(viewportHeight * 0.68) - panelHeight / 2;
    const int panelY = qBound(0, desiredY, qMax(0, viewportHeight - panelHeight));
    const QRect panel((viewportWidth - panelWidth) / 2, panelY, panelWidth, panelHeight);

    const int horizontalInset = qMin(18, qMax(0, (panel.width() - 1) / 2));
    const int verticalInset = qMin(10, qMax(0, (panel.height() - 1) / 2));
    return {panel,
            panel.adjusted(horizontalInset, verticalInset, -horizontalInset, -verticalInset)};
}
} // namespace ActionFeedbackGeometry
