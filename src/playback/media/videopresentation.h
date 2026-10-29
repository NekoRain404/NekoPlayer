#pragma once

enum class VideoAspect { Source, Ratio4x3, Ratio16x9, Ratio16x10, Ratio21x9, Stretch };

// Session-only accepted intent. Rotation is clockwise and added to file metadata.
struct VideoPresentation {
    VideoAspect aspect = VideoAspect::Source;
    int rotation = 0;
    bool operator==(const VideoPresentation &) const = default;
};
