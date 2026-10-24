# Appearance Themes and Glass UI Design

## Goal

Add a polished, configurable appearance system to NekoPlayer: additional color
presets and an optional translucent/softened treatment for UI surfaces. The
video image, libmpv rendering path, and decode path must remain unchanged.

## User-facing behavior

- Keep the existing Dark, Light, and System base themes.
- Add independently selectable accent presets: Ocean, Emerald, Violet, Rose,
  and Amber. Accents color interactive states (selection, focus, and controls),
  not the entire interface.
- Add an Appearance page in Preferences with base theme, accent, and a glass
  treatment switch. Applying a selection updates the current UI immediately and
  persists it for the next launch.
- Keep existing theme-menu behavior compatible; theme selection from that menu
  continues to update the same persisted base-theme preference.
- With glass treatment enabled, eligible UI surfaces use translucent layered
  fills, restrained gradients, rounded edges, and soft shadows. With it disabled,
  those surfaces use opaque colors from the selected base theme.

## Surface and playback boundary

Eligible surfaces include the welcome/history UI, playlist and other non-video
panels, menus/dialogs, and playback controls. Playback controls may have a
translucent fill while overlaying video, but the video pixels beneath them are
not blurred or otherwise transformed. The video widget remains opaque and is
excluded from appearance effects. No change is made to mpv, its OpenGL render
context, hardware decoding, HDR handling, or frame presentation.

Qt Widgets has no portable backdrop-filter API for blurring sibling widgets
inside the same window. Wayland has no universal application-controlled
protocol for that effect, and compositor window blur applies to the area behind
the top-level window, not selectively to its internal panels. Therefore the
glass switch controls the supported translucent/softened UI treatment; it must
not claim that arbitrary in-window content is truly blurred. Where a compositor
provides compatible top-level background blur, it may be used only when it does
not expose or alter the video surface. Otherwise use the same visual treatment
without backdrop blur. Do not add a separate GPU blur renderer or Vulkan
dependency for this feature.

## Architecture and settings

- Extend the existing appearance token model so base theme and accent are
  separate inputs. Keep colors centralized in `appearancetokens` and generated
  widget styling in `appearancestylesheet`.
- Extend `AppearancePreferences` as the appearance value/application boundary;
  avoid scattering palette construction or preference parsing across widgets.
- Add an Appearance category to `PreferencesDialog`. Changes should apply live
  and update persisted settings using the existing `appearance/*` namespace.
- Store base theme in `appearance/theme` (existing key), accent in
  `appearance/accent`, and the glass switch in `appearance/glassEnabled`.
  Invalid/missing accent values fall back to Ocean; a missing glass value
  defaults to enabled. Configuration-disabled launches continue not to save
  preference changes.
- Keep UI effect styling declarative and low-cost. Do not enable OpenGL or force
  hardware acceleration for the widgets. The platform compositor may accelerate
  final window composition; retain an opaque, low-cost fallback.

## Quality and verification

- Add focused tests for every accent token set, invalid preference fallback,
  persistence/application, and both glass-enabled and opaque style generation.
- Confirm that changing appearance does not mutate video-widget attributes,
  mpv render settings, hardware-decoding preferences, or HDR settings.
- Run the normal build and non-window test suite. Window behavior tests remain
  skipped, per the user's standing preference.
- Keep handwritten source files under 500 lines unless a documented exception
  is justified; do not introduce a file over 800 lines.

## Out of scope

True GPU backdrop blur of application-internal content, changing video
transparency, new playback rendering/decoding features, Vulkan integration, and
redesign of unrelated playback behavior.
