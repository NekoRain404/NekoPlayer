# NekoPlayer Release-Readiness Plan

**Goal:** Move NekoPlayer from a feature-complete prototype toward a coherent release by fixing the remaining preferences-entry defect, adding configurable desktop interaction, polishing high-visibility UI details, and synchronizing the documentation with the code.

**Spec:** `docs/superpowers/specs/2026-10-24-release-readiness-design.md`

**Baseline:** `feat/first-playable` at the commit that finishes the System-palette accent fix. All work stays on this branch; no worktree, no push, no merge.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; keep the single playback engine and the existing render path.
- Keep playback state/commands, graphics resources, and desktop interaction in separate layers. UI code must not issue raw mpv commands.
- Ordinary handwritten files target 500 lines; the automated guard tolerates up to 525. `src/ui/mainwindow.cpp` and `tests/window_test.cpp` keep their 800-line exception. Split by responsibility, never by line count.
- Do not run window-behavior tests in the default suite; compile them. The standing verification selection is:
  `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'`
- Use `cmake --preset debug`, `cmake --build --preset debug`, `ctest --preset debug`.
- Every commit and review artifact must use only the existing `NekoRain` git identity. Do not add co-author trailers.
- Preserve the user-owned `docs/skills.md` working-tree change; never stage it.
- `references/` is read-only study material. Do not copy upstream code.
- Update `translations/NekoPlayer_en.ts` for every new visible string and verify the catalog compiles.
- After each task: focused tests, `code-size`, `clang-format --dry-run --Werror` on changed files, `git diff --check`, then commit.

---

## Task 1: Unified Preferences entry and coordinator

**Why:** `SubtitleDisplayMenu` still creates its own `PreferencesDialog` without current appearance, media-library, resume, or seek values. Changes made through F6 are preview-only and can overwrite real preferences with defaults.

**Files:**
- Create `src/ui/preferencescoordinator.h`
- Create `src/ui/preferencescoordinator.cpp`
- Modify `src/ui/subtitledisplaymenu.h`
- Modify `src/ui/subtitledisplaymenu.cpp`
- Modify `src/ui/windowactions.cpp`
- Modify `src/ui/mainwindow.h`
- Modify `CMakeLists.txt`
- Create `tests/preferences_coordinator_test.cpp`
- Modify `tests/CMakeLists.txt`
- Modify `translations/NekoPlayer_en.ts` only if new visible strings appear.

**Interfaces:**
- `PreferencesCoordinator` is a `QObject` owned by `MainWindow`.
- It receives a `MpvController &` and a `Handlers` struct of narrow getters/setters:
  `appearance`, `setAppearance`, `mediaLibrary`, `setMediaLibrary`, `resumePlaybackPosition`, `setResumePlaybackPosition`, `seekStepSeconds`, `setSeekStepSeconds`.
- `std::unique_ptr<PreferencesDialog> createDialog(QWidget *parent, PreferencesDialog::Category category)` builds a fully wired dialog with current values and applies `selectCategory`.
- `void show(QWidget *parent, PreferencesDialog::Category category)` calls `createDialog` and `exec()`.
- `SubtitleDisplayMenu` takes a `std::function<void(PreferencesDialog::Category)> showPreferences` argument (default empty) and uses it for the F6/settings action instead of constructing a dialog.
- `MainWindow` builds the handlers with lambdas that read `session_` lazily, so `--no-config` still gets session-only changes.

**Steps:**
- [ ] Add the coordinator and its tests before wiring the menus.
- [ ] Replace the `showPreferences` lambda in `windowactions.cpp` with the coordinator; pass its callback to `SubtitleDisplayMenu`.
- [ ] Keep `PreferencesDialog` as the only page implementation; do not duplicate page construction.
- [ ] Test: current values reach the dialog; Apply propagates each preference through its handler; F6 invokes the shared callback with `Category::Subtitles`.
- [ ] Run focused `preferences-coordinator`, `preferences-dialog`, `appearance-preferences`, `code-size`; build all.
- [ ] Commit as `fix(ui): unify preferences entry and preserve current values`.

## Task 2: Interaction preferences persistence and settings page

**Why:** The player needs configurable desktop behavior (click, double-click, wheel, middle-click) instead of hard-coded defaults.

**Files:**
- Modify `src/session/sessionstate.h`
- Modify `src/session/sessionstore.cpp`
- Modify `src/session/sessionmanager.h`
- Modify `src/session/sessionmanager.cpp`
- Modify `src/ui/preferencesdialog.h`
- Modify `src/ui/preferencesdialog.cpp`
- Modify `src/ui/preferencescoordinator.h`
- Modify `src/ui/preferencescoordinator.cpp`
- Modify `src/ui/windowactions.cpp`
- Modify `tests/session_test.cpp`
- Modify `tests/preferences_dialog_test.cpp`
- Modify `tests/preferences_coordinator_test.cpp`
- Modify `translations/NekoPlayer_en.ts`

**Interfaces:**
- New value type:
  ```cpp
  enum class MiddleClickAction { None, PlayPause, Mute, Fullscreen };
  struct InteractionPreferences {
      bool singleClickPlayPause = true;
      bool doubleClickFullscreen = true;
      MiddleClickAction middleClick = MiddleClickAction::None;
      bool wheelAdjustsVolume = true;
      int wheelVolumeStep = 5; // 1..20
      bool wheelSeeksTimeline = true;
      bool playlistSingleClick = false;
  };
  ```
- `SessionState` gains `InteractionPreferences interaction;`.
- `SessionStore` reads/writes optional `interaction/*` keys; missing or invalid values fall back independently; `wheelVolumeStep` is clamped to 1..20; `middleClick` accepts `none`, `playPause`, `mute`, `fullscreen` and falls back to `none`.
- `SessionManager` exposes `interactionPreferences()` and `setInteractionPreferences()`.
- `PreferencesDialog` gains `Category::Interaction` as a new final category, a constructor argument `InteractionPreferences interaction = {}`, and `interactionPreferencesChanged(InteractionPreferences)`.
- The Interaction page has named, accessible controls: `interactionSingleClickChoice`, `interactionDoubleClickChoice`, `interactionMiddleClickChoice`, `interactionWheelVolumeChoice`, `interactionWheelStepChoice`, `interactionWheelSeekChoice`, `interactionPlaylistClickChoice`. Apply emits once when changed; Restore Defaults on this page resets the defaults.
- `PreferencesCoordinator::Handlers` gains the interaction getter/setter.

**Steps:**
- [ ] Add failing session-store tests for defaults, round trip, invalid step clamping, and old session files.
- [ ] Add failing dialog tests for the sixth category, control values, Apply signal, and Restore Defaults.
- [ ] Implement persistence and the page; keep existing category indices stable.
- [ ] Wire the coordinator and Settings menu.
- [ ] Add English translations for the new strings.
- [ ] Run `session`, `preferences-dialog`, `preferences-coordinator`, `code-size`; build all.
- [ ] Commit as `feat(settings): add configurable playback interaction`.

## Task 3: Mouse and wheel behavior

**Why:** The new interaction settings must actually affect the video surface and timeline, with visible feedback and no changes to the mpv render path.

**Files:**
- Create `src/ui/interactionpolicy.h`
- Create `src/ui/interactionpolicy.cpp`
- Modify `src/playback/videowidget.h`
- Modify `src/playback/videowidget.cpp`
- Modify `src/ui/transportbar.h`
- Modify `src/ui/transportbar.cpp`
- Modify `src/ui/mainwindow.h`
- Modify `src/ui/mainwindow.cpp`
- Modify `src/ui/windowactions.cpp` only if signal wiring belongs there.
- Create `tests/interaction_policy_test.cpp`
- Modify `tests/transport_bar_test.cpp`
- Modify `tests/CMakeLists.txt`

**Interfaces:**
- `interactionpolicy.h` exposes pure helpers:
  - `int wheelNotches(int deltaY)` using 120 units per notch with a minimum magnitude of 1 for nonzero deltas.
  - `double volumeAfterWheel(double current, int deltaY, int step)` clamped to 0..100.
- `VideoWidget` adds signals `singleClicked()`, `middleClicked()`, `wheelScrolled(int deltaY)`, and a single-shot timer so a single left click is not also reported before a double click.
- `VideoWidget::mouseDoubleClickEvent` cancels the single-click timer and emits `doubleClicked()`.
- `TransportBar` overrides timeline wheel handling and emits `wheelSeekRequested(int direction)` where direction is -1/1; it never lets QSlider perform a hidden one-step seek.
- `MainWindow` stores the active `InteractionPreferences`, applies it on restore and on settings changes, and:
  - toggles playback on single click when enabled;
  - toggles fullscreen on double click when enabled;
  - performs the configured middle-click action (`None`, `PlayPause`, `Mute`, `Fullscreen`);
  - adjusts volume by `wheelVolumeStep` per notch when enabled;
  - seeks by the configured seek step on timeline wheel when enabled.
- Feedback continues to use the existing `ActionFeedbackOverlay` / seek feedback.

**Steps:**
- [ ] Add policy tests and a transport-bar wheel signal test first.
- [ ] Implement the widget signals and timer; keep `VideoWidget` free of mpv and preferences logic.
- [ ] Wire MainWindow; do not add polling or frame copies.
- [ ] Run `interaction-policy`, `transport-bar`, `preferences-dialog`, `session`, `code-size`; build all.
- [ ] Commit as `feat(ui): add configurable click and wheel interaction`.

## Task 4: High-visibility UI polish

**Why:** Small, reversible details make the player feel finished without touching playback or rendering.

**Files:**
- Modify `src/ui/transportbar.h`
- Modify `src/ui/transportbar.cpp`
- Modify `src/ui/appearanceicons.h`
- Modify `src/ui/appearanceicons.cpp`
- Modify `src/ui/windowactions.cpp`
- Modify `tests/transport_bar_test.cpp`
- Modify `tests/ui_surface_test.cpp` only if the icon contract needs it.

**Interfaces / behavior:**
- The transport time label becomes clickable: clicking toggles between elapsed/total and `-remaining / total`. It exposes an accessible description and a tooltip; keyboard focus and activation are supported.
- Add a `Settings` symbol to `AppearanceIcons` and use it for `playbackSettingsAction`, `mediaLibrarySettingsAction`, and `appearanceSettingsAction`; keep the existing icon for About.
- Do not alter layout metrics, QSS selectors, or the fullscreen fade behavior.

**Steps:**
- [ ] Add a transport-bar test for the time-mode toggle and the new tooltip/accessible text.
- [ ] Add a focused icon test if the new symbol is not covered by existing contracts.
- [ ] Implement the label toggle and settings icons.
- [ ] Run `transport-bar`, `appearance-preferences`, `ui-surface`, `code-size`; build all.
- [ ] Commit as `feat(ui): polish transport time and settings icons`.

## Task 5: Playlist ergonomics

**Why:** The playlist is a primary surface, but it currently has a fixed width, double-click-only activation, and no quick way to find an item in a long queue.

**Files:**
- Modify `src/ui/playlistpane.h`
- Modify `src/ui/playlistpane.cpp`
- Modify `src/ui/mainwindow.cpp`
- Modify `src/ui/mainwindow.h` if the splitter needs a member
- Modify `src/session/sessionstate.h`
- Modify `src/session/sessionstore.cpp`
- Modify `src/session/sessionmanager.h`
- Modify `src/session/sessionmanager.cpp`
- Modify `src/ui/preferencescoordinator.cpp` only if the coordinator must expose the new value
- Modify `tests/session_test.cpp`
- Modify `tests/playlist_edit_test.cpp` only if activation behavior is covered there
- Modify `translations/NekoPlayer_en.ts`

**Interfaces / behavior:**
- `PlaylistPane::setSingleClickActivation(bool)` connects `QListView::clicked` to the same play request as `activated` when enabled; double-click and Enter keep working.
- The video/playlist row uses a `QSplitter` with the existing playlist minimum width (240 px), `setCollapsible(1, false)`, and video stretch 1.
- `WindowPreferences` gains `int playlistWidth = 0`; 0 means the default. Persist the width when the playlist is visible and restore it after the splitter is built. Clamp to a sensible range on load (240–1000).
- Fullscreen/mini-player continue to hide the playlist without changing the stored width.
- Add a header search field? Do not add a proxy model in this phase: it would complicate current-row identity, drag/drop, and failure history. Record the search field as a follow-up instead.

**Steps:**
- [ ] Add session-store tests for default/invalid/round-trip playlist width.
- [ ] Add a focused PlaylistPane test for single-click activation if it can be done without a live renderer.
- [ ] Replace the fixed horizontal layout with the splitter; restore and save width.
- [ ] Wire `playlistSingleClick` from `InteractionPreferences`.
- [ ] Run `session`, `playlist-edit`, `transport-bar`, `code-size`; build all.
- [ ] Commit as `feat(ui): make the playlist resizable and click-configurable`.

## Task 6: Performance and boundary cleanup

**Why:** The audit found per-tick work on the GUI thread and a duplicated appearance apply. These are cheap to fix and directly improve responsiveness and layering.

**Files:**
- Modify `src/ui/windowactions.cpp`
- Modify `src/ui/mainwindow.cpp`
- Modify `src/ui/mainwindow.h`
- Modify `src/ui/preferencesdialog.cpp`
- Modify `src/ui/foldermenu.cpp`
- Modify `src/ui/appearancepreferences.cpp`
- Modify `src/ui/backenddiagnostic.h` / `.cpp` or `src/playback/backenddiagnostic.cpp`
- Modify `tests/window_test.cpp` only if a compile-only assertion is needed
- Modify `tests/preferences_dialog_test.cpp` / `tests/ui_surface_test.cpp` for focused coverage.

**Interfaces / behavior:**
- `MainWindow::updateState()` must not allocate a new icon on every `time-pos` tick. `setAppearanceIcon` caches the symbol and palette; skip `AppearanceIcons::icon` when unchanged. Cache the window title by path instead of rebuilding `MediaSource::displayName` on every state tick.
- `PreferencesDialog` must not recompute every draft on each playback tick. Gate `updateApplyButton()`/`updateActualDecoder()` on a cached `(active, hardwareDecoder)` and on dedicated preference signals; playback position changes alone must not drive dialog work.
- Appearance Apply must apply once: the preview already applies the draft. `PreferencesDialog::apply()` updates its rollback snapshots and emits; `AppearanceMenu::setSettings()` remains the single apply/persist owner. Add a focused test that the final settings and persistence are correct.
- `FolderMenu::setMediaLibraryPreferences()` must block programmatic `toggled` signals so the session is not written twice.
- Move the libmpv client API version query out of `appearancepreferences.cpp` into `BackendDiagnostic` (or an existing playback-layer helper) so the UI layer does not include `mpv/client.h` for About text.
- Do not change the render path, polling behavior, or playback semantics.

**Steps:**
- [ ] Add focused regressions for icon/title stability across repeated `updateState()` ticks and for preference-dialog state gating where feasible without window tests.
- [ ] Implement the caches and single-owner appearance apply.
- [ ] Move the client API version query behind the playback boundary.
- [ ] Run focused `preferences-dialog`, `appearance-preferences`, `ui-surface`, `code-size`; build all.
- [ ] Commit as `perf(ui): remove per-tick churn and duplicated applies`.

## Task 7: Documentation, verification, and final review

**Files:**
- Modify `README.md`
- Modify `docs/architecture.md`
- Modify `docs/verification.md`
- Modify `docs/appearance-handoff.md` only where the current state changed.
- Create `docs/superpowers/specs/2026-10-24-release-readiness-design.md` if it is not already present.

**Steps:**
- [ ] Document the unified preferences entry, interaction settings, time toggle, and current line-count policy.
- [ ] Record exact commands and results for Debug non-window tests, focused sanitizer tests, formatting, and `git diff --check`.
- [ ] Run a whole-branch review with a fresh reviewer subagent; fix or record every Critical/Important finding.
- [ ] Run the complete non-window Debug suite; run the relevant sanitizer tests for session/preferences changes.
- [ ] Commit as `docs: record release-readiness verification`.

## Verification Checklist

- [ ] `cmake --build --preset debug -j4` succeeds.
- [ ] `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passes.
- [ ] `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passes.
- [ ] Focused sanitizer tests for session/preferences/policy pass with leak detection disabled only for documented platform-library noise.
- [ ] `clang-format --dry-run --Werror` on changed files and `git diff --check` pass.
- [ ] Window behavior tests compile but are not run.
- [ ] No raw mpv commands in UI code; no CPU frame copies or new polling.
