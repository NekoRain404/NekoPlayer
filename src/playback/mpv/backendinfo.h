#pragma once

#include <QString>

namespace BackendInfo {
// libmpv client API version reported as "major.minor", e.g. "2.2".
[[nodiscard]] QString clientApiVersion();
} // namespace BackendInfo
