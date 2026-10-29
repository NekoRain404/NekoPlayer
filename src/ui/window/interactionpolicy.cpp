#include "ui/window/interactionpolicy.h"

#include <algorithm>

int wheelNotches(int deltaY) {
    if (deltaY == 0)
        return 0;
    const int notches = deltaY / 120;
    // High-resolution wheels and touchpads can report less than one notch;
    // treat any nonzero delta as one step in the reported direction.
    return notches != 0 ? notches : (deltaY > 0 ? 1 : -1);
}

double volumeAfterWheel(double current, int deltaY, int step) {
    const auto target = current + static_cast<double>(wheelNotches(deltaY) * step);
    return std::clamp(target, 0.0, 100.0);
}
