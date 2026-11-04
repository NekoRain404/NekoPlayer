#include "playback/mpv/mpvcontroller_p.h"

ReplayGainMode MpvController::replayGainMode() const { return impl_->replayGainMode; }

bool MpvController::audioExclusive() const { return impl_->audioExclusive; }

void MpvController::setReplayGainMode(ReplayGainMode mode) {
    auto &self = *impl_;
    const char *value = mode == ReplayGainMode::Track ? "track"
                        : mode == ReplayGainMode::Album ? "album"
                                                           : "no";
    const int status = mpv_set_property_string(self.client.get(), "replaygain", value);
    self.report(status);
    if (status < 0 || self.replayGainMode == mode)
        return;
    self.replayGainMode = mode;
    emit replayGainModeChanged();
}

void MpvController::setAudioExclusive(bool enabled) {
    auto &self = *impl_;
    const int status =
        mpv_set_property_string(self.client.get(), "audio-exclusive", enabled ? "yes" : "no");
    self.report(status);
    if (status < 0 || self.audioExclusive == enabled)
        return;
    self.audioExclusive = enabled;
    emit audioExclusiveChanged();
}
