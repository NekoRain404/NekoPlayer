#include "mpvcontroller_p.h"

const SubtitlePreferences &MpvController::subtitlePreferences() const { return impl_->subtitles; }

void MpvController::setSubtitlePreferences(const SubtitlePreferences &preferences) {
    const auto target = preferences.normalized();
    auto &current = impl_->subtitles;
    bool changed = false;
    const auto submit = [this, &changed](const char *name, const auto &targetValue,
                                         auto &currentValue, const QByteArray &argument) {
        if (targetValue != currentValue && impl_->command({"set", name, argument})) {
            currentValue = targetValue;
            changed = true;
        }
    };
    // Commands copy their arguments before returning. Store accepted intent now,
    // so immediate saves and rapid shortcuts do not depend on event delivery.
    submit("sub-visibility", target.visible, current.visible, target.visible ? "yes" : "no");
    submit("sub-scale", target.scale, current.scale, QByteArray::number(target.scale, 'g', 15));
    submit("sub-pos", target.position, current.position,
           QByteArray::number(target.position, 'g', 15));
    submit("sub-font", target.font, current.font, target.font.toUtf8());
    submit("sub-color", target.textColor, current.textColor, target.textColor.toUtf8());
    submit("sub-outline-color", target.outlineColor, current.outlineColor,
           target.outlineColor.toUtf8());
    submit("sub-outline-size", target.outlineSize, current.outlineSize,
           QByteArray::number(target.outlineSize, 'g', 15));
    submit("sub-shadow-offset", target.shadowOffset, current.shadowOffset,
           QByteArray::number(target.shadowOffset, 'g', 15));
    submit("sub-back-color", target.backgroundColor, current.backgroundColor,
           target.backgroundColor.toUtf8());
    submit("sub-codepage", target.codepage, current.codepage, target.codepage.toUtf8());
    if (changed)
        emit subtitlePreferencesChanged();
}

void MpvController::setSubtitleVisible(bool visible) {
    auto preferences = subtitlePreferences();
    preferences.visible = visible;
    setSubtitlePreferences(preferences);
}

void MpvController::setSubtitleScale(double scale) {
    if (!std::isfinite(scale))
        return;
    auto preferences = subtitlePreferences();
    preferences.scale = scale;
    setSubtitlePreferences(preferences);
}

void MpvController::adjustSubtitleScale(double delta) {
    setSubtitleScale(subtitlePreferences().scale + delta);
}

void MpvController::setSubtitlePosition(double position) {
    if (!std::isfinite(position))
        return;
    auto preferences = subtitlePreferences();
    preferences.position = position;
    setSubtitlePreferences(preferences);
}

void MpvController::adjustSubtitlePosition(double delta) {
    setSubtitlePosition(subtitlePreferences().position + delta);
}

void MpvController::setSubtitleFont(const QString &font) {
    auto preferences = subtitlePreferences();
    preferences.font = font;
    setSubtitlePreferences(preferences);
}
