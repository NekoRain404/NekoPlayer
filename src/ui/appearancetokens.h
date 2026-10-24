#pragma once

#include "appearancepreferences.h"

#include <QColor>
#include <QPalette>

struct AppearanceTokens {
    QColor canvas;
    QColor surface;
    QColor raisedSurface;
    QColor primaryText;
    QColor secondaryText;
    QColor mutedText;
    QColor separator;
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
    int controlRadius = 6;
    int surfaceRadius = 10;
    int transportHitTarget = 32;
};

[[nodiscard]] AppearanceTokens
appearanceTokens(AppearancePreferences::Theme theme,
                 AppearancePreferences::Accent accent = AppearancePreferences::Accent::Ocean);
[[nodiscard]] QPalette paletteForTokens(const AppearanceTokens &tokens);
