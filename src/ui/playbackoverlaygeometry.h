#pragma once

#include <QRect>
#include <QSize>

namespace PlaybackOverlayGeometry {
struct Layout {
    QRect information;
    QRect seek;
};

inline int boundedExtent(int requested, int available) {
    if (available <= 0)
        return 0;
    return qBound(1, requested, available);
}

inline Layout arrange(const QSize &viewport, const QSize &informationHint, const QSize &seekHint) {
    const int viewportWidth = qMax(0, viewport.width());
    const int viewportHeight = qMax(0, viewport.height());

    // Keep the usual 20 px breathing room, but yield it before a panel can
    // escape a very small fullscreen surface.
    const int horizontalInset = qMin(20, qMax(0, (viewportWidth - 1) / 2));
    const int informationWidth =
        boundedExtent(informationHint.width(), qMax(0, viewportWidth - 2 * horizontalInset));
    const int seekWidth =
        boundedExtent(seekHint.width(), qMax(0, viewportWidth - 2 * horizontalInset));

    const int topInset = qMin(20, qMax(0, (viewportHeight - 1) / 2));
    const int informationHeight =
        boundedExtent(informationHint.height(), qMax(0, viewportHeight - topInset));
    const int seekHeight = boundedExtent(seekHint.height(), viewportHeight);
    const int bottomInset = qMin(32, qMax(0, viewportHeight - seekHeight));

    return {QRect(horizontalInset, topInset, informationWidth, informationHeight),
            QRect((viewportWidth - seekWidth) / 2, viewportHeight - seekHeight - bottomInset,
                  seekWidth, seekHeight)};
}
} // namespace PlaybackOverlayGeometry
