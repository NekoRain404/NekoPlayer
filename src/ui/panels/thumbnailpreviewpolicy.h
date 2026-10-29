#pragma once

#include "playback/mpv/mpvcontroller.h"

[[nodiscard]] bool canRequestThumbnailPreview(const PlaybackState &state, bool hasMotionVideo);
