#pragma once

#include <optional>

// Latest accepted endpoints for this media; execution errors are reported separately.
struct AbLoop {
    std::optional<double> start;
    std::optional<double> end;
    bool operator==(const AbLoop &) const = default;
};
