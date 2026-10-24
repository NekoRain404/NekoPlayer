# Media-First UI Visual System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a compact, reusable visual system and apply it consistently to the main window's existing menus/status area, welcome panel, playlist pane, and transport bar without changing playback behavior.

**Architecture:** Keep theme selection and persistence in `AppearancePreferences`; move semantic colors/metrics and stylesheet construction into small UI-only modules. Widgets continue to own layout and behavior, consume `QPalette`/stable object names for appearance, and retain their existing `QAction` bindings. Add a centralized, high-DPI-safe monochrome icon painter for icons shown on the covered surfaces.

**Tech Stack:** C++20, Qt 6.5 Widgets/Gui/Test, CMake, CTest, clang-format, native Wayland Qt platform plugin.

**Spec:** `docs/superpowers/specs/2026-10-24-media-first-ui-design.md`

## Global Constraints

- Use C++20 / Qt 6 Widgets / libmpv; retain Wayland as the primary desktop target.
- Ordinary handwritten code files must remain at or below 500 lines; only `src/ui/mainwindow.cpp` and `tests/window_test.cpp` have the existing 800-line exception.
- Preserve `AppearancePreferences::Theme { Dark, Light, System }`, existing QSettings format, Qt Widgets, `QAction` behavior, libmpv, native Wayland window decoration, and every playback behavior.
- Keep visual tokens in the UI layer; do not add media-frame reads, playback subscriptions, I/O, mpv options, or persistence to visual code.
- Preserve at least 32 px transport action hit targets and high-DPI vector/painter rendering.
- This phase does not restructure the home/history information architecture, playback layout/overlay, fullscreen behavior, mini-player, or preference dialogs.
- Run focused tests, full Debug build, formatting, and code-size checks; do not run window-behavior tests by default. Distinguish Xvfb layout checks from real Wayland visual acceptance.
- Commit the completed phase with the configured NekoRain identity after verification.

---

## File Map

- Create `src/ui/appearancetokens.h/.cpp`: semantic color roles, spacing/radius/size metrics, and Dark/Light values. No Qt Widgets or playback dependencies.
- Create `src/ui/appearancestylesheet.h/.cpp`: convert a token set into the application stylesheet for covered surfaces.
- Create `src/ui/appearanceicons.h/.cpp`: draw the covered surfaces' monochrome icons with `QPainter`/`QIconEngine`, honoring palette roles and device-pixel ratio.
- Modify `src/ui/appearancepreferences.cpp`: retain theme naming/about behavior and delegate palette/style creation; `System` continues to clear the application stylesheet and use the desktop palette.
- Modify `src/ui/windowactions.cpp`, `src/ui/mainwindow.cpp`, `src/ui/playlistpane.cpp`, and `src/ui/welcomepanel.cpp`: request semantic icons from the shared helper instead of independently selecting platform standard icons where those icons appear in this phase. Do not change action connections or playback state.
- Modify `src/ui/transportbar.cpp` only if its current geometry needs token-backed spacing; keep `TransportBarMetrics` and its 32 px compact targets intact.
- Modify `CMakeLists.txt` to register new UI sources; modify `tests/CMakeLists.txt` only if a separate focused token/icon test target is needed.
- Extend `tests/appearance_preferences_test.cpp` and `tests/ui_surface_test.cpp` with semantic-theme, state, icon, and regression assertions. Keep these files below 500 lines; split tests into a new focused test file if they would exceed that limit.

## Task 1: Define Semantic Theme Tokens

**Files:**
- Create: `src/ui/appearancetokens.h`
- Create: `src/ui/appearancetokens.cpp`
- Modify: `src/ui/appearancepreferences.cpp`
- Modify: `CMakeLists.txt`
- Modify: `tests/appearance_preferences_test.cpp`
- Test: `tests/appearance_preferences_test.cpp`

**Interfaces:**
- Produces `struct AppearanceTokens` with `QColor` roles `canvas`, `surface`, `raisedSurface`, `primaryText`, `secondaryText`, `mutedText`, `separator`, `accent`, `accentHover`, `accentPressed`, `focus`, `selection`, `selectedText`, `disabledText`, `hoverSurface`, and `pressedSurface`, plus integer `spaceUnit`, `controlRadius`, `surfaceRadius`, and `transportHitTarget`.
- Produces `[[nodiscard]] AppearanceTokens appearanceTokens(AppearancePreferences::Theme theme);` for Dark and Light. System is not a custom token palette; callers use the application palette and do not request custom tokens for it.
- Produces `[[nodiscard]] QPalette paletteForTokens(const AppearanceTokens &tokens);` for the `AppearancePreferences` coordinator.
- `AppearancePreferences::applyTheme(QApplication &, Theme)` remains the public entry point and retains existing `Theme::System` behavior.

- [x] **Step 1: Add failing token contract tests**

Include `ui/appearancetokens.h` in `tests/appearance_preferences_test.cpp`, then add this test:

In `AppearancePreferencesTest`, add a test that checks role ordering/readability and shared geometry values without asserting brittle RGB constants:

```cpp
void darkAndLightTokensKeepSemanticContrastAndSharedMetrics() {
    const auto dark = appearanceTokens(AppearancePreferences::Theme::Dark);
    const auto light = appearanceTokens(AppearancePreferences::Theme::Light);
    QVERIFY(dark.canvas.value() < dark.primaryText.value());
    QVERIFY(light.canvas.value() > light.primaryText.value());
    QVERIFY(dark.accent != dark.canvas);
    QVERIFY(light.accent != light.canvas);
    QCOMPARE(dark.spaceUnit, light.spaceUnit);
    QCOMPARE(dark.transportHitTarget, 32);
}
```

- [x] **Step 2: Run the focused test and confirm the missing API fails to compile**

Run: `cmake --build --preset debug --target appearance_preferences_test -j4`

Expected: compilation fails because `AppearanceTokens` and `appearanceTokens` have not been introduced.

- [x] **Step 3: Implement the token value type and two semantic palettes**

Define the exact fields above in `appearancetokens.h`; initialize one complete value per custom theme in `appearancetokens.cpp`. Keep shared geometric values equal between themes (`spaceUnit = 4`, `transportHitTarget = 32`) and use distinct 8 px main spacing and restrained radius values. Keep every authored file under 500 lines.

Use this value-object shape (the implementation supplies every listed role):

```cpp
struct AppearanceTokens {
    QColor canvas;
    QColor surface;
    QColor raisedSurface;
    QColor primaryText;
    QColor secondaryText;
    QColor mutedText;
    QColor separator;
    QColor accent;
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
```

- [x] **Step 4: Make the existing theme entry point consume token-derived palettes**

Move palette role assignment out of `appearancepreferences.cpp` into `QPalette paletteForTokens(const AppearanceTokens &tokens)` declared by `appearancetokens.h`. `applyTheme` uses it for Dark/Light; System calls `application.setStyleSheet({})` and restores the platform style's standard palette behavior as it does today.

- [x] **Step 5: Build and run the focused contract test**

Run: `cmake --build --preset debug --target appearance_preferences_test -j4 && ctest --test-dir build/debug -R '^appearance-preferences$' --output-on-failure`

Expected: target builds and `appearance-preferences` passes for all three theme modes.

- [x] **Step 6: Commit the token boundary**

```bash
git add CMakeLists.txt src/ui/appearancetokens.h src/ui/appearancetokens.cpp src/ui/appearancepreferences.cpp tests/appearance_preferences_test.cpp
git commit -m "refactor(ui): define semantic appearance tokens"
```

## Task 2: Generate Main-Window Styling From Tokens

**Files:**
- Create: `src/ui/appearancestylesheet.h`
- Create: `src/ui/appearancestylesheet.cpp`
- Modify: `src/ui/appearancepreferences.cpp`
- Modify: `CMakeLists.txt`
- Test: `tests/appearance_preferences_test.cpp`

**Interfaces:**
- Produces `[[nodiscard]] QString appearanceStyleSheet(const AppearanceTokens &tokens);`.
- Styles only stable existing selectors: `QMainWindow`, `QMenuBar`, `QMenu`, `QStatusBar`, `QWidget#transportBar`, `QWidget#playlistPane`, `QWidget#playlistHeader`, `QWidget#welcomePanel`, existing named labels/buttons/sliders, `QListView#playlistView`, and keyboard focus/state pseudo-selectors.
- Does not add selectors that depend on text, translated labels, widget ancestry traversal, or playback properties.

- [x] **Step 1: Strengthen the existing style test around both themes and required states**

Extend `exposesContentFirstInteractionStyle()` so each theme's generated sheet contains stable selectors for hover, pressed, checked, selected, focus, and disabled states on its relevant controls. Preserve the current behavior assertions for named selectors and timeline/volume controls. Do not assert internal stylesheet ownership; verify user-visible generated selectors and theme behavior instead.

- [x] **Step 2: Run the focused test to capture the missing generator**

Run: `cmake --build --preset debug --target appearance_preferences_test -j4`

Expected: compilation fails because `appearanceStyleSheet` is not declared yet.

- [x] **Step 3: Extract the stylesheet and interpolate semantic values**

Implement `appearanceStyleSheet` in a dedicated file. Encode every color using `QColor::name(QColor::HexRgb)` from the token roles; encode spacing and radii from the token metrics. Remove duplicate hard-coded theme sheets from `appearancepreferences.cpp` and keep that coordinator below 500 lines. Preserve all existing object selectors relied upon by `transportbar.cpp`, `playlistpane.cpp`, `welcomepanel.cpp`, and tests.

For example, generate the transport surface from token values rather than a second color table:

```cpp
return QStringLiteral("QWidget#transportBar { background: %1; color: %2; "
                      "border-top: 1px solid %3; }")
    .arg(tokens.canvas.name(QColor::HexRgb), tokens.primaryText.name(QColor::HexRgb),
         tokens.separator.name(QColor::HexRgb));
```

- [x] **Step 4: Apply measured, restrained styling without layout or action rewiring**

Use one spacing rhythm (4 px base / 8 px primary rhythm), quiet separators, smaller consistent surface radii, readable primary/secondary text, a restrained accent for primary/selected/focus states, and visible disabled/hover/pressed/checked states. Keep `System` free of the app stylesheet. Do not change widget layout, shortcut, enablement, media queue, or signal wiring in this task.

- [x] **Step 5: Verify selectors and theme switching**

Run: `cmake --build --preset debug --target appearance_preferences_test ui_surface_test -j4 && ctest --test-dir build/debug -R '^(appearance-preferences|ui-surface)$' --output-on-failure`

Expected: both tests pass; applying System after Dark or Light leaves `qApp->styleSheet()` empty, and Dark/Light styles include the shared surface and interaction selectors.

- [x] **Step 6: Commit the stylesheet extraction**

```bash
git add CMakeLists.txt src/ui/appearancestylesheet.h src/ui/appearancestylesheet.cpp src/ui/appearancepreferences.cpp tests/appearance_preferences_test.cpp
git commit -m "feat(ui): apply semantic media-first theme styling"
```

## Task 3: Normalize Covered Surface Icons and Component Contracts

**Files:**
- Create: `src/ui/appearanceicons.h`
- Create: `src/ui/appearanceicons.cpp`
- Modify: `src/ui/windowactions.cpp`
- Modify: `src/ui/mainwindow.cpp`
- Modify: `src/ui/mainwindow.h` to declare a private icon-refresh helper if required by the existing state-update organization
- Modify: `src/ui/playlistpane.cpp`
- Modify: `src/ui/welcomepanel.cpp`
- Modify: `src/ui/transportbar.cpp` only if a visual token is needed for existing spacing
- Modify: `CMakeLists.txt` to register the icon source
- Test: `tests/ui_surface_test.cpp`, `tests/appearance_preferences_test.cpp`

**Interfaces:**
- Produces `QIcon AppearanceIcons::icon(AppearanceIcons::Symbol symbol, const QPalette &palette);` with symbols for open, network, playlist, save, add, stop, previous, play, pause, next, fullscreen, restore, seek, volume, mute, remove, move-up, move-down, information, and media-file.
- Icon drawing uses logical coordinates and `QPainter` paths with `QIconEngine` pixmap generation at the requested device-pixel ratio; colors come from `QPalette::ButtonText` or `QPalette::WindowText`, with disabled state generated from `QPalette::Disabled` roles.
- Widgets continue to bind the same `QAction` instances. Icon replacement must not change shortcuts, labels, accessibility names, signal paths, or enabled/checked state.

- [x] **Step 1: Add icon-contract checks to `ui_surface_test`**

Include `ui/appearanceicons.h` and add the following kind of assertions using `QIcon::pixmap(QSize(24, 24), 2.0)`:

```cpp
void sharedIconsRenderAtDevicePixelRatio() {
    QPalette palette;
    palette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#70a0ff")));
    const auto play = AppearanceIcons::icon(AppearanceIcons::Symbol::Play, palette);
    QVERIFY(!play.isNull());
    const auto native = play.pixmap(QSize(24, 24));
    QVERIFY(!native.isNull());
    QCOMPARE(native.size(), QSize(24, 24));
    const auto highDpi = play.pixmap(QSize(24, 24), 2.0);
    QVERIFY(!highDpi.isNull());
    QCOMPARE(highDpi.size(), QSize(48, 48));
    QCOMPARE(highDpi.devicePixelRatio(), 2.0);
}
```

Repeat for a media-file and one disabled-mode icon. Existing transport button count, minimum size, accessible names, default actions, playlist editing, and welcome action tests remain intact.

- [x] **Step 2: Run `ui-surface` and confirm the icon factory contract is absent**

Run: `cmake --build --preset debug --target ui_surface_test -j4`

Expected: compilation fails on the new `AppearanceIcons` interface.

- [x] **Step 3: Implement the small vector icon engine**

Implement only the enumerated symbols, grouped into reusable path primitives for play/transport, file/media, and common actions. Paint with antialiasing in a normalized 24 × 24 logical canvas; apply transforms rather than raster scaling. Keep the header and source each below 500 lines, and avoid emoji, external runtime assets, SVG plugin requirements, and platform-specific window glyphs.

Keep the public symbol list explicit and independent of translated menu text:

```cpp
namespace AppearanceIcons {
enum class Symbol {
    Open, Network, Playlist, Save, Add, Stop, Previous, Play, Pause, Next,
    Fullscreen, Restore, Seek, Volume, Mute, Remove, MoveUp, MoveDown,
    Information, MediaFile
};
QIcon icon(Symbol symbol, const QPalette &palette);
}
```

The engine's paint path uses the requested palette mode and scales the normalized painter before dispatch:

```cpp
const auto color = mode == QIcon::Disabled
                       ? palette.color(QPalette::Disabled, QPalette::ButtonText)
                       : palette.color(QPalette::ButtonText);
painter->save();
painter->setRenderHint(QPainter::Antialiasing);
painter->scale(rect.width() / 24.0, rect.height() / 24.0);
painter->setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
paintSymbol(*painter, symbol_);
painter->restore();
```

- [x] **Step 4: Route covered widgets through the shared icons**

Replace direct `QStyle::standardIcon` selection in the covered construction/update sites with `AppearanceIcons::icon`. For the dynamic play/pause, mute, and fullscreen/restore actions, preserve the existing state decision and only change the icon provider. `WelcomePanel` continues copying the action icon, and all buttons continue using `setDefaultAction`/existing QAction binding. Palette icon refresh belongs in the existing `MainWindow::changeEvent` implementation in `src/ui/windowactions.cpp`; update only the icon on `QEvent::PaletteChange`, while leaving its `LanguageChange` branch unchanged.

Refresh palette-colored action icons from `MainWindow::changeEvent` on `QEvent::PaletteChange` as well as at action creation/state changes, so switching Dark/Light/System updates already-visible menu, toolbar, playlist, and welcome icons without restarting. Do not add a timer or playback-property observer for this refresh.

- [x] **Step 5: Run visual-surface and regression tests**

Run: `cmake --build --preset debug --target appearance_preferences_test ui_surface_test -j4 && ctest --test-dir build/debug -R '^(appearance-preferences|ui-surface)$' --output-on-failure`

Expected: both tests pass; icon tests pass under offscreen rendering; action and accessibility regression tests remain unchanged and pass.

- [x] **Step 6: Commit the shared icon/component contract**

```bash
git add CMakeLists.txt src/ui/appearanceicons.h src/ui/appearanceicons.cpp src/ui/windowactions.cpp src/ui/mainwindow.cpp src/ui/playlistpane.cpp src/ui/welcomepanel.cpp src/ui/transportbar.cpp tests/ui_surface_test.cpp tests/appearance_preferences_test.cpp
git commit -m "feat(ui): unify main window iconography"
```

## Task 4: Verify the First UI Phase and Record Acceptance Evidence

**Files:**
- Modify: `docs/verification.md` with dated, factual commands and results after they are run.
- Review: all files from Tasks 1–3 and the design spec.

- [x] **Step 1: Format and check whitespace**

Run: `clang-format -i` on only the changed `.cpp`/`.h` files, then `git diff --check`.

Expected: no whitespace errors; formatting does not alter behavior.

- [x] **Step 2: Run all focused visual/theme and code-size checks**

Run: `cmake --build --preset debug -j4 && ctest --test-dir build/debug -R '^(code-size|appearance-preferences|ui-surface)$' --output-on-failure`

Expected: complete Debug build succeeds and all three tests pass. Do not run window-behavior tests in this default selection.

- [ ] **Step 3: Perform native Wayland visual acceptance when a Wayland session is available**

Launch `./build/debug/NekoPlayer --no-config` from the native Wayland session. Inspect Dark and Light welcome surfaces and a playing window at 16:9, a narrow window, and 1.75× display scaling. Confirm text is not clipped, actions retain their hit targets, focused/disabled/selected states remain distinct, the video position/layout has not changed, and there is no visible icon raster blur. If only Xvfb/offscreen is available, record that as layout-only evidence and mark native Wayland visual acceptance as not performed rather than claiming it passed.

- [x] **Step 4: Recheck all code-size limits and commit the completed phase**

Run: `ctest --test-dir build/debug -R '^code-size$' --output-on-failure && git diff --check && git status --short`

Expected: code-size passes; every ordinary handwritten file is at or below 500 lines, and only the two registered files may be up to 800 lines. Add the actual verification result to `docs/verification.md`, then commit:

```bash
git add docs/verification.md
git commit -m "docs: record media-first UI verification"
```

If this is the last commit for the phase, combine it with the implementation commit policy in the handoff so the phase has one clear Git milestone; never commit before the verification outcomes are known.

## Self-Review

- Spec coverage: semantic palette and shared styling are Tasks 1–2; main-window surfaces and icon consistency are Task 3; theme switching, Dark/Light/System, interaction states, width/scaling, text, Wayland distinction, non-regression, and line limits are Task 4.
- Scope: no homepage information-architecture changes, playback-state re-layout, fullscreen work, mini-player, settings-dialog restyle, libmpv changes, or behavior rewiring are included.
- Placeholder scan: all tasks name files, APIs, assertions, commands, and expected outcomes; no TBD/TODO steps are present.
- Interface consistency: `appearanceTokens(Theme)` returns the value consumed by `paletteForTokens` and `appearanceStyleSheet`; `AppearanceIcons::icon(Symbol, QPalette)` is used consistently for covered static and dynamic icon sites.
- File-size safety: each new production module is split by responsibility; test growth must trigger a new focused test source before crossing the 500-line limit.
