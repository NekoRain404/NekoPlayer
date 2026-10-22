#include "videocoloradjustment.h"

#include <algorithm>
#include <cmath>

VideoColorAdjustment VideoColorAdjustment::normalized() const {
    const auto clamp = [](double value) {
        return std::isfinite(value) ? std::clamp(value, -100.0, 100.0) : 0.0;
    };
    return {clamp(brightness), clamp(contrast), clamp(saturation), clamp(hue), clamp(gamma)};
}
