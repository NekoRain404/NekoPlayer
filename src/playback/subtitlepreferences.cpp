#include "subtitlepreferences.h"

#include <algorithm>
#include <cmath>

SubtitlePreferences SubtitlePreferences::normalized() const {
    auto result = *this;
    result.scale = std::isfinite(scale) ? std::clamp(scale, 0.25, 4.0) : 1;
    result.position = std::isfinite(position) ? std::clamp(position, 0.0, 100.0) : 100;
    result.font = font.trimmed();
    if (result.font.isEmpty() || result.font.size() > 256 ||
        std::any_of(result.font.cbegin(), result.font.cend(),
                    [](QChar character) { return character.category() == QChar::Other_Control; }))
        result.font = QStringLiteral("sans-serif");
    return result;
}
