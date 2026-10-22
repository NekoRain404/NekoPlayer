#pragma once

#include <QString>
#include <optional>

std::optional<double> parsePlaybackTime(const QString &text);
QString formatPlaybackTime(double seconds, bool includeMilliseconds = false);
