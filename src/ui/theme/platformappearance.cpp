#include "ui/theme/platformappearance.h"

#include <QApplication>
#include <QPalette>
#include <QWidget>

#ifdef Q_OS_WIN
// windows.h must precede dwmapi.h (the latter relies on HWND/BOOL).
// clang-format off
#include <windows.h>
#include <dwmapi.h>
// clang-format on
#endif

namespace {
#ifdef Q_OS_WIN
constexpr DWORD immersiveDarkMode = 20;
// Windows 10 builds before 18985 only understand the legacy attribute.
constexpr DWORD immersiveDarkModeLegacy = 19;

bool applicationUsesDarkPalette() {
    const auto *application = qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!application)
        return false;
    return application->palette().color(QPalette::Window).lightness() < 128;
}

void applyDarkTitleBar(QWidget &window, bool dark) {
    const auto handle = reinterpret_cast<HWND>(window.winId());
    if (!handle)
        return;
    const BOOL enabled = dark ? TRUE : FALSE;
    if (SUCCEEDED(DwmSetWindowAttribute(handle, immersiveDarkMode, &enabled, sizeof(enabled))))
        return;
    // Older Windows 10 builds expose the same setting under attribute 19.
    DwmSetWindowAttribute(handle, immersiveDarkModeLegacy, &enabled, sizeof(enabled));
}
#endif
} // namespace

void PlatformAppearance::enableGlassSurface(QWidget &widget) {
#ifdef Q_OS_WIN
    // A translucent top-level window with a native frame can render black or
    // lose its shadow. Keep the surface opaque so the QSS base colors show.
    Q_UNUSED(widget)
#else
    widget.setAttribute(Qt::WA_TranslucentBackground, true);
#endif
}

void PlatformAppearance::apply(QWidget &window) {
#ifdef Q_OS_WIN
    applyDarkTitleBar(window, applicationUsesDarkPalette());
#else
    Q_UNUSED(window)
#endif
}
