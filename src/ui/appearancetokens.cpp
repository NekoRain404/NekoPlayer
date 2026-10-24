#include "appearancetokens.h"

namespace {
AppearanceTokens darkTokens() {
    return {QColor(31, 34, 40),  QColor(22, 24, 29),  QColor(38, 41, 48),
            QColor(232, 234, 238), QColor(191, 199, 216), QColor(146, 154, 169),
            QColor(59, 65, 77),  QColor(139, 169, 239), QColor(161, 187, 248),
            QColor(112, 145, 215), QColor(139, 169, 239), QColor(53, 68, 95),
            QColor(244, 247, 255), QColor(119, 123, 133), QColor(52, 56, 66),
            QColor(69, 75, 88), 4, 6, 10, 32};
}

AppearanceTokens lightTokens() {
    return {QColor(245, 246, 248), QColor(255, 255, 255), QColor(240, 241, 244),
            QColor(32, 35, 40),   QColor(85, 91, 100),   QColor(133, 139, 149),
            QColor(214, 219, 227), QColor(48, 125, 220), QColor(62, 139, 231),
            QColor(40, 107, 192), QColor(48, 125, 220), QColor(215, 229, 251),
            QColor(23, 59, 112), QColor(154, 157, 162), QColor(230, 233, 238),
            QColor(215, 220, 228), 4, 6, 10, 32};
}
} // namespace

AppearanceTokens appearanceTokens(AppearancePreferences::Theme theme) {
    return theme == AppearancePreferences::Theme::Dark ? darkTokens() : lightTokens();
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
