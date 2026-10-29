#pragma once

struct VideoColorAdjustment {
    double brightness = 0;
    double contrast = 0;
    double saturation = 0;
    double hue = 0;
    double gamma = 0;

    [[nodiscard]] VideoColorAdjustment normalized() const;
    bool operator==(const VideoColorAdjustment &) const = default;
};
