#pragma once

#include "ui/theme/appearancepreferences.h"

#include <QColor>
#include <QPalette>

struct AppearanceTokens {
    QColor canvas;
    QColor surface;
    QColor raisedSurface;
    QColor accentSurface;
    QColor primaryText;
    QColor secondaryText;
    QColor mutedText;
    QColor separator;
    QColor border;
    QColor accent;
    QColor accentText;
    QColor accentHover;
    QColor accentPressed;
    QColor focus;
    QColor selection;
    QColor selectedText;
    QColor disabledText;
    QColor hoverSurface;
    QColor pressedSurface;
    int spaceUnit = 4;
    int controlRadius = 8;
    int surfaceRadius = 12;
    int controlHeight = 36;
    int rowHeight = 32;
    int menuItemHeight = 32;
    int captionFontPixels = 11;
    int compactFontPixels = 12;
    int bodyFontPixels = 14;
    int largeFontPixels = 16;
    int motionDurationMs = 160;
    int transportHitTarget = 32;
};

[[nodiscard]] AppearanceTokens
appearanceTokens(AppearancePreferences::Theme theme,
                 AppearancePreferences::Accent accent = AppearancePreferences::Accent::Ocean);
[[nodiscard]] QPalette paletteForTokens(const AppearanceTokens &tokens);
