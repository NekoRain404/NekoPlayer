#pragma once

namespace TransportBarMetrics {
struct Metrics {
    int buttonExtent = 36;
    int volumeWidth = 110;
    int timeWidth = 155;
    int minimumRowWidth = 0;
    bool showTime = true;
    bool showVolume = true;
};

inline Metrics forWidth(int width) {
    Metrics metrics;
    if (width < 360) {
        metrics.buttonExtent = 28;
        metrics.showTime = false;
        metrics.showVolume = false;
    } else if (width < 480) {
        metrics.buttonExtent = 28;
        metrics.volumeWidth = 60;
        metrics.showTime = false;
    } else if (width < 640) {
        metrics.buttonExtent = 32;
        metrics.volumeWidth = 76;
        metrics.timeWidth = 100;
    }

    // The row has 12 layout gaps, two explicit separators and 12 px margins.
    metrics.minimumRowWidth = 7 * metrics.buttonExtent + 48 + 18 + 24;
    metrics.minimumRowWidth += metrics.showTime ? metrics.timeWidth : 0;
    metrics.minimumRowWidth += metrics.showVolume ? metrics.volumeWidth : 0;
    return metrics;
}
} // namespace TransportBarMetrics
