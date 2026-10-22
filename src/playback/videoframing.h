#pragma once

// Zoom is log2 scale; pan is a fraction of the scaled video dimensions.
struct VideoFraming {
    double zoom = 0;
    double panX = 0;
    double panY = 0;

    bool operator==(const VideoFraming &) const = default;
};
