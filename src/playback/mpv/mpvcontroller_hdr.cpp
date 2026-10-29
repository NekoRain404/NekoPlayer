#include "playback/mpv/mpvcontroller_p.h"

HdrToneMapping MpvController::hdrToneMapping() const { return impl_->hdrToneMapping; }

void MpvController::setHdrToneMapping(HdrToneMapping mapping) {
    const char *value = nullptr;
    switch (mapping) {
    case HdrToneMapping::Auto:
        value = "auto";
        break;
    case HdrToneMapping::Bt2390:
        value = "bt.2390";
        break;
    case HdrToneMapping::Clip:
        value = "clip";
        break;
    default:
        return;
    }

    // Keep the accepted UI intent in sync only when libmpv accepted the
    // asynchronous command. The backend remains the source of rendered HDR
    // pixels; no video frame is copied through the CPU.
    if (!impl_->command({"set", "tone-mapping", value}))
        return;
    if (impl_->hdrToneMapping == mapping)
        return;
    impl_->hdrToneMapping = mapping;
    emit hdrToneMappingChanged();
}
