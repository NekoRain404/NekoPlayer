#pragma once

#include <QString>

namespace MediaSource {
// Returns an absolute local path or an encoded network URL; empty means invalid input.
[[nodiscard]] QString normalize(const QString &source);
[[nodiscard]] QString networkUrl(const QString &input);
[[nodiscard]] bool isNetwork(const QString &source);
[[nodiscard]] QString displayName(const QString &source);
} // namespace MediaSource
