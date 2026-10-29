#include "ui/window/uimotionduration.h"

#include <QtGlobal>

int uiMotionDuration(int requestedDurationMs, bool reduceMotion) {
    if (reduceMotion)
        return 0;
    return qMax(0, requestedDurationMs);
}

QEasingCurve uiMotionEasingCurve() { return QEasingCurve(QEasingCurve::OutCubic); }
