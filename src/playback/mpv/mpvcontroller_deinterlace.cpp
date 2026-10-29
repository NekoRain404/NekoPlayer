#include "playback/mpv/mpvcontroller_p.h"

DeinterlaceMode MpvController::deinterlaceMode() const { return impl_->deinterlaceMode; }

void MpvController::setDeinterlaceMode(DeinterlaceMode mode) {
    const char *value = nullptr;
    switch (mode) {
    case DeinterlaceMode::Off:
        value = "no";
        break;
    case DeinterlaceMode::Auto:
        value = "auto";
        break;
    case DeinterlaceMode::On:
        value = "yes";
        break;
    default:
        return;
    }
    if (!impl_->command({"set", "deinterlace", value}) || impl_->deinterlaceMode == mode)
        return;
    impl_->deinterlaceMode = mode;
    emit deinterlaceModeChanged();
}
