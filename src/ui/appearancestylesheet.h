#pragma once

#include "appearancetokens.h"

#include <QString>

[[nodiscard]] QString appearanceStyleSheet(const AppearanceTokens &tokens,
                                           bool glassEnabled = false);
