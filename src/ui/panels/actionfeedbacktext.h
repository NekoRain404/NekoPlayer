#pragma once

#include <QString>

namespace ActionFeedbackText {
QString seek(double seconds);
QString seekTo(double seconds);
QString volume(int percentage);
QString mute(bool muted);
QString playback(bool pauseRequested);
} // namespace ActionFeedbackText
