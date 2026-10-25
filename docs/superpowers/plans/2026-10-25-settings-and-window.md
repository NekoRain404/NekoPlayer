# Settings and Window Behavior Implementation Plan

**Goal:** Complete interaction settings and add always-on-top and startup flags.

**Spec:** `docs/superpowers/specs/2026-10-25-settings-and-window-design.md`

**Baseline:** `feat/first-playable` at the end of the playlist/discovery phase.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; no playback/render change beyond the explicit pause-on-start behavior.
- Ordinary files target 500 lines; hard guard 550; `mainwindow.cpp` / `window_test.cpp` keep 800.
- Wayland only; window tests compile only.
- `LC_ALL=C LANG=C` for ctest.
- Commits only under NekoRain; no co-author trailers.
- Preserve `docs/skills.md`; `references/` read-only.
- Use subagents for implementation and review.

---

## Task 1: Interaction settings completion

**Files:** `src/session/sessionstate.h`, `sessionstore.cpp`, `src/ui/preferencesdialog.{h,cpp}`, `src/ui/preferencescoordinator.cpp`, `src/ui/windowactions.cpp`, `src/ui/transportbar.{h,cpp}`, `src/ui/mainwindow.cpp`, tests, translations.

**Behavior:**
- Keyboard volume +/- uses `InteractionPreferences::wheelVolumeStep`; relabel the control to “音量步进”.
- Add `showRemainingTime = false` to `InteractionPreferences`; key `interaction/showRemainingTime`; optional schema-1; checkbox `interactionRemainingTimeChoice`.
- Add `TransportBar::setTimeShowsRemaining(bool)` and getter; MainWindow applies on restore and when interaction settings change; click toggle stays transient.
- Tests: defaults/round-trip/malformed, dialog controls/Apply, coordinator values, transport setter.

**Commit:** `feat(settings): complete interaction preferences`.

## Task 2: Always-on-top

**Files:** `src/session/sessionstate.h`, `sessionstore.cpp`, `sessionmanager.{h,cpp}`, `src/ui/mainwindow.{h,cpp}`, `src/ui/windowsession.cpp`, `src/ui/windowactions.cpp`, tests.

**Behavior:**
- `WindowPreferences::alwaysOnTop = false`; key `window/alwaysOnTop`; session round-trip.
- View menu checkable `alwaysOnTopAction`; `setAlwaysOnTop` toggles `Qt::WindowStaysOnTopHint` and re-shows while preserving geometry.
- Mini-player entry forces on-top; exit restores `alwaysOnTop_` rather than false.
- Tests: session round-trip; compile-only window assertions for action/flag/mode restoration.

**Commit:** `feat(ui): add a persisted always-on-top toggle`.

## Task 3: CLI startup options

**Files:** `src/main.cpp`, `src/cli/commandlinearguments.{h,cpp}` if a helper is useful, `src/ui/mainwindow.{h,cpp}`, tests.

**Behavior:**
- `--fullscreen`: after `show()`, call `showFullScreen()`; existing WindowStateChange sync applies.
- `--start-paused`: set a MainWindow flag before opening files; pause once after the first `fileLoaded`.
- Keep `--check-backend`, `--smoke-test`, `--no-config`, `--silent-audio`.
- Tests: parser/helper coverage where possible; compile-only window assertions.

**Commit:** `feat(cli): add fullscreen and start-paused startup options`.

## Task 4: Verification/docs/final review

- Full non-window suite, focused sanitizers, code-size, formatting, diff check, translations.
- Update README/architecture/verification/plan.
- Fresh whole-branch reviewer, one fix wave if needed.
- Commit docs.

## Status (2026-10-25)

- [x] Task 1 — interaction settings completion (`5baf6ef`): shared `音量步进` for wheel and keyboard +/-, persisted `interaction/showRemainingTime`, transient click override.
- [x] Task 2 — persisted always-on-top with mini-player restoration (`1896d2b`); test registration split by responsibility (`cb584c9`).
- [x] Task 3 — `--fullscreen` / `--start-paused` startup options (`1ebb538`).
- [x] Task 4 — reviewed start-paused rejection fix (`0edf2fe`), full verification, and documentation.
- [ ] Native-Wayland visual/keyboard/pointer acceptance — requires an interactive Wayland desktop session.

Verification summary: `cmake --preset debug` and `cmake --build --preset debug` succeeded; the non-window Debug suite passed **61/61** (58.24 s) under `LC_ALL=C LANG=C`; the focused session/preferences/transport/code-size selection passed **6/6**; focused ASan/UBSan passed **5/5** with `ASAN_OPTIONS=detect_leaks=0` (platform-library exit-allocation caveat, not a leak-free claim); `code-size` passed 1/1 with advisory over-target files `src/ui/windowactions.cpp` (534) and `tests/ui_surface_test.cpp` (550); translations reported **296 finished, 0 unfinished**; formatting and `git diff --check` were clean; install produced exactly three files. Window/startup tests were compiled only, not run.
