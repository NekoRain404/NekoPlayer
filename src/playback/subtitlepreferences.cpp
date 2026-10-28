#include "subtitlepreferences.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {
QString normalizedColor(const QString &value, const QString &fallback) {
    static const QRegularExpression pattern(
        QStringLiteral("\\A#(?:[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})\\z"));
    return pattern.match(value).hasMatch() ? value.toUpper() : fallback;
}

QString normalizedCodepage(const QString &value) {
    const auto trimmed = value.trimmed();
    if (trimmed.isEmpty() || trimmed.size() > 64 ||
        std::any_of(trimmed.cbegin(), trimmed.cend(),
                    [](QChar character) { return character.category() == QChar::Other_Control; }))
        return QStringLiteral("auto");
    return trimmed;
}
} // namespace

SubtitlePreferences SubtitlePreferences::normalized() const {
    auto result = *this;
    result.scale = std::isfinite(scale) ? std::clamp(scale, 0.25, 4.0) : 1;
    result.position = std::isfinite(position) ? std::clamp(position, 0.0, 100.0) : 100;
    result.font = font.trimmed();
    if (result.font.isEmpty() || result.font.size() > 256 ||
        std::any_of(result.font.cbegin(), result.font.cend(),
                    [](QChar character) { return character.category() == QChar::Other_Control; }))
        result.font = QStringLiteral("sans-serif");
    result.textColor = normalizedColor(result.textColor, QStringLiteral("#FFFFFFFF"));
    result.outlineColor = normalizedColor(result.outlineColor, QStringLiteral("#FF000000"));
    result.backgroundColor = normalizedColor(result.backgroundColor, QStringLiteral("#AF000000"));
    result.outlineSize = std::isfinite(outlineSize) ? std::clamp(outlineSize, 0.0, 10.0) : 1.65;
    result.shadowOffset = std::isfinite(shadowOffset) ? std::clamp(shadowOffset, 0.0, 10.0) : 0.0;
    result.codepage = normalizedCodepage(result.codepage);
    return result;
}
