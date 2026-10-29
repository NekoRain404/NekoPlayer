#pragma once

#include "ui/theme/appearancetokens.h"

#include <QString>

[[nodiscard]] QString appearanceStyleSheet(const AppearanceTokens &tokens,
                                           bool glassEnabled = false,
                                           int glassOpacity = glassOpacityDefault,
                                           int glassBlur = glassBlurDefault);
