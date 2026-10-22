#include "mpvcontroller_p.h"

const VideoColorAdjustment &MpvController::videoColorAdjustment() const {
    return impl_->videoColor;
}

void MpvController::setVideoColorAdjustment(const VideoColorAdjustment &adjustment) {
    const auto target = adjustment.normalized();
    auto &current = impl_->videoColor;
    bool changed = false;
    const auto submit = [this, &changed](const char *name, double value, double &stored) {
        if (value == stored)
            return;
        const auto argument = QByteArray::number(value, 'g', 15);
        if (impl_->command({"set", name, argument})) {
            stored = value;
            changed = true;
        }
    };
    submit("brightness", target.brightness, current.brightness);
    submit("contrast", target.contrast, current.contrast);
    submit("saturation", target.saturation, current.saturation);
    submit("hue", target.hue, current.hue);
    submit("gamma", target.gamma, current.gamma);
    if (changed)
        emit videoColorAdjustmentChanged();
}
