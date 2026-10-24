#include "thumbnailpreviewpolicy.h"
#include "preview/thumbnailservice.h"

#include <cmath>

bool canRequestThumbnailPreview(const PlaybackState &state, bool hasMotionVideo) {
    return hasMotionVideo && state.active && state.seekable && std::isfinite(state.duration) &&
           state.duration > 0.0 && ThumbnailService::isLocalSource(state.path);
}
