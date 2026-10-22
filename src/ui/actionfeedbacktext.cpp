#include "actionfeedbacktext.h"

#include <QCoreApplication>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace ActionFeedbackText {
QString seek(double seconds) {
    if (!std::isfinite(seconds) || qFuzzyIsNull(seconds))
        return {};
    const auto magnitude = qRound64(std::abs(seconds));
    const auto pattern = seconds > 0 ? "前进 %1 秒" : "后退 %1 秒";
    return QCoreApplication::translate("ActionFeedbackText", pattern).arg(magnitude);
}

QString seekTo(double seconds) {
    if (!std::isfinite(seconds))
        return {};
    const auto total = static_cast<qint64>(std::max(0.0, seconds));
    const auto time = QStringLiteral("%1:%2:%3")
                          .arg(total / 3600, 2, 10, QLatin1Char('0'))
                          .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
                          .arg(total % 60, 2, 10, QLatin1Char('0'));
    return QCoreApplication::translate("ActionFeedbackText", "跳转至 %1").arg(time);
}

QString volume(int percentage) {
    const auto bounded = std::clamp(percentage, 0, 100);
    return QCoreApplication::translate("ActionFeedbackText", "音量 %1%").arg(bounded);
}

QString mute(bool muted) {
    return QCoreApplication::translate("ActionFeedbackText", muted ? "已静音" : "取消静音");
}

QString playback(bool pauseRequested) {
    return QCoreApplication::translate("ActionFeedbackText",
                                       pauseRequested ? "已暂停" : "继续播放");
}
} // namespace ActionFeedbackText
