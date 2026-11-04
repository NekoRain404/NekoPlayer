#pragma once

class QWidget;

namespace PlatformAppearance {
// Enables the compositor-backed translucent glass surface. Does nothing on
// Windows, where an opaque surface avoids a black frame and a missing native
// window shadow.
void enableGlassSurface(QWidget &widget);

// Applies native window chrome. On Windows this toggles the DWM dark title
// bar according to the application palette (the theme that
// AppearancePreferences applied); on every other platform it is a no-op.
void apply(QWidget &window);
} // namespace PlatformAppearance
