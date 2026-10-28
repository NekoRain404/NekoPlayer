#include "mainwindow.h"

#include "appearanceicons.h"

#include <QAction>

namespace {
constexpr auto appearanceIconSymbolProperty = "appearanceIconSymbol";
constexpr auto appearanceIconPaletteCacheProperty = "appearanceIconPaletteCache";
} // namespace

void MainWindow::setAppearanceIcon(QAction *action, AppearanceIcons::Symbol symbol) {
    const auto paletteCacheKey = palette().cacheKey();
    const auto storedSymbol = action->property(appearanceIconSymbolProperty);
    const auto storedPaletteCache = action->property(appearanceIconPaletteCacheProperty);
    if (storedSymbol.isValid() && storedPaletteCache.isValid() &&
        storedSymbol.toInt() == static_cast<int>(symbol) &&
        storedPaletteCache.toLongLong() == paletteCacheKey)
        return;
    action->setProperty(appearanceIconSymbolProperty, static_cast<int>(symbol));
    action->setProperty(appearanceIconPaletteCacheProperty, paletteCacheKey);
    action->setIcon(AppearanceIcons::icon(symbol, palette()));
}

void MainWindow::refreshAppearanceIcons() {
    for (auto *action : findChildren<QAction *>()) {
        const auto symbol = action->property(appearanceIconSymbolProperty);
        if (symbol.isValid())
            setAppearanceIcon(action, static_cast<AppearanceIcons::Symbol>(symbol.toInt()));
    }
}
