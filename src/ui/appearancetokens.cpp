#include "appearancetokens.h"

#include <QApplication>
#include <QStyle>

namespace {
QColor mix(const QColor &base, const QColor &overlay, qreal amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + overlay.redF() * amount,
                            base.greenF() * (1 - amount) + overlay.greenF() * amount,
                            base.blueF() * (1 - amount) + overlay.blueF() * amount);
}

QColor accentColor(AppearancePreferences::Accent accent, bool dark) {
    using Accent = AppearancePreferences::Accent;
    switch (accent) {
    case Accent::Emerald:
        return dark ? QColor("#73cfb0") : QColor("#187653");
    case Accent::Violet:
        return dark ? QColor("#b59aee") : QColor("#7751ad");
    case Accent::Rose:
        return dark ? QColor("#ec96b7") : QColor("#b33c6d");
    case Accent::Amber:
        return dark ? QColor("#e4b86a") : QColor("#886011");
    case Accent::Ocean:
        return dark ? QColor("#8ba9ef") : QColor("#286fc5");
    }
    return dark ? QColor("#8ba9ef") : QColor("#286fc5");
}

AppearanceTokens darkTokens() {
    AppearanceTokens tokens;
    tokens.canvas = QColor(31, 34, 40);
    tokens.surface = QColor(22, 24, 29);
    tokens.raisedSurface = QColor(38, 41, 48);
    tokens.primaryText = QColor(232, 234, 238);
    tokens.secondaryText = QColor(191, 199, 216);
    tokens.mutedText = QColor(146, 154, 169);
    tokens.separator = QColor(59, 65, 77);
    tokens.disabledText = QColor(119, 123, 133);
    tokens.hoverSurface = QColor(52, 56, 66);
    tokens.pressedSurface = QColor(69, 75, 88);
    return tokens;
}

AppearanceTokens lightTokens() {
    AppearanceTokens tokens;
    tokens.canvas = QColor(245, 246, 248);
    tokens.surface = QColor(255, 255, 255);
    tokens.raisedSurface = QColor(240, 241, 244);
    tokens.primaryText = QColor(32, 35, 40);
    tokens.secondaryText = QColor(85, 91, 100);
    tokens.mutedText = QColor(104, 112, 122);
    tokens.separator = QColor(214, 219, 227);
    tokens.disabledText = QColor(154, 157, 162);
    tokens.hoverSurface = QColor(230, 233, 238);
    tokens.pressedSurface = QColor(215, 220, 228);
    return tokens;
}
} // namespace

AppearanceTokens appearanceTokens(AppearancePreferences::Theme theme,
                                  AppearancePreferences::Accent accent) {
    auto tokens = theme == AppearancePreferences::Theme::Dark ? darkTokens() : lightTokens();
    if (theme == AppearancePreferences::Theme::System) {
        // Always start from the style's standard palette, never our previously applied palette.
        const auto palette = QApplication::style()->standardPalette();
        tokens.canvas = palette.color(QPalette::Window);
        tokens.surface = palette.color(QPalette::Base);
        tokens.raisedSurface = palette.color(QPalette::AlternateBase);
        tokens.primaryText = palette.color(QPalette::WindowText);
        tokens.secondaryText = palette.color(QPalette::Text);
        tokens.mutedText = palette.color(QPalette::PlaceholderText);
        tokens.separator = palette.color(QPalette::Mid);
        tokens.disabledText = palette.color(QPalette::Disabled, QPalette::Text);
        tokens.hoverSurface = palette.color(QPalette::Button);
        tokens.pressedSurface = palette.color(QPalette::Midlight);
    }
    const auto dark = tokens.canvas.lightnessF() < 0.5;
    tokens.accent = accentColor(accent, dark);
    tokens.accentText = dark ? QColor("#1f2228") : QColor(Qt::white);
    tokens.accentHover = dark ? tokens.accent.lighter(110) : tokens.accent.darker(110);
    tokens.accentPressed = dark ? tokens.accent.darker(110) : tokens.accent.darker(120);
    tokens.focus = tokens.accent;
    tokens.selection = mix(tokens.canvas, tokens.accent, dark ? 0.24 : 0.16);
    tokens.selectedText = tokens.primaryText;
    return tokens;
}

QPalette paletteForTokens(const AppearanceTokens &tokens) {
    QPalette palette;
    palette.setColor(QPalette::Window, tokens.canvas);
    palette.setColor(QPalette::WindowText, tokens.primaryText);
    palette.setColor(QPalette::Base, tokens.surface);
    palette.setColor(QPalette::AlternateBase, tokens.raisedSurface);
    palette.setColor(QPalette::ToolTipBase, tokens.raisedSurface);
    palette.setColor(QPalette::ToolTipText, tokens.primaryText);
    palette.setColor(QPalette::Text, tokens.primaryText);
    palette.setColor(QPalette::Button, tokens.surface);
    palette.setColor(QPalette::ButtonText, tokens.primaryText);
    palette.setColor(QPalette::BrightText, tokens.accentPressed);
    palette.setColor(QPalette::Link, tokens.accent);
    palette.setColor(QPalette::Highlight, tokens.selection);
    palette.setColor(QPalette::HighlightedText, tokens.selectedText);
    palette.setColor(QPalette::Disabled, QPalette::Text, tokens.disabledText);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, tokens.disabledText);
    return palette;
}
