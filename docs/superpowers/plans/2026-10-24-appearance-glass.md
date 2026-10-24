# Appearance Themes and Glass UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add selectable accent palettes and an optional polished translucent UI treatment without altering video presentation or decoding.

**Architecture:** Extend the centralized appearance tokens/styles with separate base-theme, accent, and glass inputs. Add an Appearance preferences category that previews changes and persists them through the existing `appearance/*` settings; keep the current theme menu compatible. Effects remain limited to UI widgets and use an opaque fallback, with no new rendering backend or forced GPU path.

**Tech Stack:** C++20, Qt 6 Widgets, QSettings, Qt Test, CMake.

**Spec:** `docs/superpowers/specs/2026-10-24-appearance-glass-design.md`

## Global Constraints

- The video image, libmpv rendering path, and decode path must remain unchanged.
- Playback controls may have a translucent fill while overlaying video, but the video pixels beneath them are not blurred or otherwise transformed.
- True GPU backdrop blur of application-internal content, changing video transparency, new playback rendering/decoding features, Vulkan integration, and redesign of unrelated playback behavior are out of scope.
- Keep handwritten source files under 500 lines unless a documented exception is justified; do not introduce a file over 800 lines.
- Run the normal build and non-window test suite. Window behavior tests remain skipped, per the user's standing preference.

---

### Task 1: Accent-aware appearance tokens and styles

**Files:**
- Modify: `src/ui/appearancepreferences.h`
- Modify: `src/ui/appearancetokens.h`
- Modify: `src/ui/appearancetokens.cpp`
- Modify: `src/ui/appearancestylesheet.h`
- Modify: `src/ui/appearancestylesheet.cpp`
- Modify: `tests/appearance_preferences_test.cpp`

**Interfaces:**
- Add `AppearancePreferences::Accent { Ocean, Emerald, Violet, Rose, Amber }` and helpers to enumerate/name/parse accent values.
- Derive tokens from the selected base theme and accent, then produce glass-enabled or opaque style values; System uses the platform standard palette as its base and still honors the selected accent.
- Generate opaque style values when glass is disabled and layered UI-only fills when enabled. Do not apply alpha or effects to the video widget.

- [ ] **Step 1: Add failing token tests** for all five accents in dark and light bases, asserting shared base surface colors, distinct accent colors, preserved contrast, and System retaining the application standard base palette while honoring its accent.
- [ ] **Step 2: Add failing stylesheet tests** asserting glass-enabled UI rules contain translucent panel fills while the disabled form is opaque; assert no style targets `VideoWidget` or an OpenGL/video surface.
- [ ] **Step 3: Run the focused appearance test** with `ctest --test-dir build/debug -R '^appearance-preferences$' --output-on-failure` and confirm the new API/tests fail before implementation.
- [ ] **Step 4: Implement the accent enum, labels, parser fallback, token derivation, and glass-aware stylesheet** in the listed appearance files. Keep accent-specific values centralized rather than branching in widgets.
- [ ] **Step 5: Build and run the focused appearance test**, then verify every new source file remains below 500 lines.
- [ ] **Step 6: Commit** as `feat: add accent palettes and glass UI tokens`.

### Task 2: Appearance preference model and startup/menu persistence

**Files:**
- Modify: `src/ui/appearancepreferences.h`
- Modify: `src/ui/appearancepreferences.cpp`
- Modify: `src/main.cpp`
- Modify: `src/ui/windowactions.cpp`
- Modify: `tests/appearance_preferences_test.cpp`

**Interfaces:**
- Add `AppearancePreferences::Settings` with `Theme theme`, `Accent accent`, and `bool glassEnabled` fields and defaults of Dark, Ocean, and enabled.
- Add `loadSettings(QSettings&)`, `saveSettings(QSettings&, const Settings&)`, and `applySettings(QApplication&, const Settings&)` so startup, menu, and Preferences use one interpretation of keys.
- Keep `appearance/theme`; add `appearance/accent` and `appearance/glassEnabled`. Unknown accent keys resolve to Ocean. When configuration is disabled, do not persist changes.

- [ ] **Step 1: Add failing QSettings tests** using a temporary INI file for defaults, round-trip persistence, invalid accent fallback, and independent theme/accent/glass values.
- [ ] **Step 2: Add a failing application test** confirming applying System restores the standard palette and applying Dark/Light uses the requested accent and glass stylesheet.
- [ ] **Step 3: Run the focused appearance test** with `ctest --test-dir build/debug -R '^appearance-preferences$' --output-on-failure` and confirm the new model/helper behavior is absent.
- [ ] **Step 4: Implement `Settings` and the three settings helpers** in `AppearancePreferences`; do not add QSettings parsing to widgets.
- [ ] **Step 5: Update startup in `main.cpp`** to load/apply all appearance values once while preserving `--no-config` behavior.
- [ ] **Step 6: Update the existing theme menu** to read the common settings model and apply/save only the changed base theme while preserving selected action state.
- [ ] **Step 7: Run appearance tests and build**, then commit as `feat: persist appearance settings`.

### Task 3: Appearance page with immediate preview and apply/revert behavior

**Files:**
- Modify: `src/ui/preferencesdialog.h`
- Modify: `src/ui/preferencesdialog.cpp`
- Modify: `src/ui/windowactions.cpp`
- Modify: `tests/preferences_dialog_test.cpp`
- Modify: `translations/NekoPlayer_en.ts`

**Interfaces:**
- Add `PreferencesDialog::Category::Appearance` as a stable final category after MediaLibrary; existing category indices remain unchanged.
- Add optional initial appearance settings to `PreferencesDialog` and an `appearancePreferencesChanged(Settings)` signal emitted after applying values.
- Expose named controls `appearanceThemeChoice`, `appearanceAccentChoice`, and `appearanceGlassChoice` for stable tests/accessibility.

- [ ] **Step 1: Add failing Preferences tests** for the fifth tab, all theme/accent options, glass checkbox default, object names/accessibility names, Apply persistence signal, and Cancel restoring the previewed appearance.
- [ ] **Step 2: Run `preferences-dialog` focused test** and confirm it fails on the missing category/controls.
- [ ] **Step 3: Add the Appearance page** with base-theme and accent combo boxes plus a glass-effect checkbox, accessible names, explanatory fallback text, and live preview using `AppearancePreferences::applySettings`.
- [ ] **Step 4: Implement Apply/Cancel semantics:** Apply emits the completed settings for persistence; Cancel restores the original appearance snapshot; Restore Defaults is available only on Appearance and resets Dark/Ocean/enabled.
- [ ] **Step 5: Connect settings-menu Preferences launch to load current values and save emitted appearance changes only when configuration is enabled.** Keep other Preferences call sites on their current defaults/selection flow.
- [ ] **Step 6: Add English translations** for every new visible string in `translations/NekoPlayer_en.ts`; use the repository's Qt translation update target if available and verify the English catalog compiles.
- [ ] **Step 7: Run focused preference/appearance tests and build**, check file lengths, and commit as `feat: add appearance preferences`.

### Task 4: UI surface coverage and playback isolation regression checks

**Files:**
- Modify: `src/ui/appearancestylesheet.cpp`
- Modify: `tests/appearance_preferences_test.cpp`
- Modify: `tests/ui_surface_test.cpp`
- Modify: `tests/CMakeLists.txt` only if an existing test target does not already include the relevant UI test.

**Interfaces:**
- Apply tokens to existing object names for welcome, playlist, menu/dialog, and transport surfaces; avoid adding visual effects to `VideoWidget`, `PlaybackOverlay`, or `ActionFeedbackOverlay`.
- Keep the fullscreen transport's existing opacity animation independent from the glass preference.

- [ ] **Step 1: Add regression assertions** that appearance styles cover the named UI panels and controls but do not install a graphics effect or translucency attribute on `VideoWidget`.
- [ ] **Step 2: Add/adjust style rules** for translucent layered fills, borders, and subtle shadows on UI-only surfaces; maintain an opaque style branch for disabled glass.
- [ ] **Step 3: Verify the transport bar changes style without changing fullscreen fade behavior** using existing non-window tests and direct style assertions.
- [ ] **Step 4: Run focused UI appearance tests**, then build and check `git diff --check`.
- [ ] **Step 5: Commit** as `feat: polish glass surfaces without affecting video`.

### Task 5: Full verification and handoff

**Files:**
- Modify: `docs/verification.md` only if the repository records new test coverage or known compositor limitations there.

- [ ] **Step 1: Configure/build the Debug tree** using the repository's existing build directory and CMake preset/command; do not create a separate worktree.
- [ ] **Step 2: Run all non-window CTest tests**, excluding targets named for window behavior as documented in `docs/verification.md`.
- [ ] **Step 3: Run the code-size test and `git diff --check`**; report any known non-blocking failures without claiming the suite passed.
- [ ] **Step 4: Inspect the final diff** to confirm no playback, mpv, HDR, or decoding files changed and the appearance settings persist correctly.
- [ ] **Step 5: Commit any documentation-only verification update** as `docs: record appearance verification`, then hand off the completed changes with exact test results.
