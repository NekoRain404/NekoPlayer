#pragma once

#include <QEasingCurve>

[[nodiscard]] int uiMotionDuration(int requestedDurationMs, bool reduceMotion);
[[nodiscard]] QEasingCurve uiMotionEasingCurve();
