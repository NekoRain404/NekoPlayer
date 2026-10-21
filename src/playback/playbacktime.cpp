#include "playbacktime.h"

#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <limits>

namespace {
// Leave headroom for floating-point rounding before converting to milliseconds.
constexpr double maximumSeconds = double(std::numeric_limits<qint64>::max() / 2000);
} // namespace

std::optional<double> parsePlaybackTime(const QString &text) {
    static const QRegularExpression syntax(
        QStringLiteral("\\A[0-9]+(?::[0-9]{1,2}){0,2}(?:\\.[0-9]{1,3})?\\z"));
    const auto trimmed = text.trimmed();
    if (trimmed.size() > 64 || !syntax.match(trimmed).hasMatch())
        return {};
    const auto parts = trimmed.split(QLatin1Char(':'));
    double seconds = 0;
    for (qsizetype index = 0; index < parts.size(); ++index) {
        bool valid = false;
        const auto value = parts[index].toDouble(&valid);
        if (!valid || (index > 0 && value >= 60))
            return {};
        seconds = seconds * 60 + value;
    }
    if (!std::isfinite(seconds) || seconds > maximumSeconds)
        return {};
    return seconds;
}

QString formatPlaybackTime(double seconds, bool includeMilliseconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > maximumSeconds)
        return QStringLiteral("--:--:--");
    const auto milliseconds = static_cast<qint64>(std::round(seconds * 1000));
    const auto total = includeMilliseconds ? milliseconds / 1000 : static_cast<qint64>(seconds);
    auto text = QStringLiteral("%1:%2:%3")
                    .arg(total / 3600, 2, 10, QLatin1Char('0'))
                    .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
                    .arg(total % 60, 2, 10, QLatin1Char('0'));
    if (includeMilliseconds)
        text += QStringLiteral(".%1").arg(milliseconds % 1000, 3, 10, QLatin1Char('0'));
    return text;
}
