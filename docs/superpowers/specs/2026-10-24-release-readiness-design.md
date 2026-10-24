# NekoPlayer Release-Readiness Design

## Intent

NekoPlayer already plays local and network media, restores sessions, manages tracks and subtitles, and has a themed desktop shell. The next step is not another broad feature branch; it is to close the defects and interaction gaps that make the product feel unfinished, while keeping the playback and rendering layers stable.

This phase therefore targets four outcomes:

1. One preferences entry that always shows and saves the real current values.
2. Configurable mouse and wheel behavior with visible, immediate feedback.
3. Small UI details that make the everyday window feel deliberate.
4. Documentation and verification that match the code.

## Goals

- Remove the split preferences paths so F6 and the Settings menu use one coordinator.
- Add persisted interaction preferences: single click, double click, middle click, wheel volume, wheel volume step, and timeline wheel seek.
- Make the video surface and timeline honor those preferences through narrow signals; keep `VideoWidget` free of mpv and preference logic.
- Add a clickable elapsed/remaining time readout and consistent settings icons.
- Keep the existing 500-line target with a small automated grace window; split by responsibility, not by line count.
- Update README, architecture, verification, and translations.

## Non-Goals

- No Qt Quick, second playback engine, plugin system, or skin system.
- No change to libmpv rendering, hardware decoding, HDR handling, or frame presentation.
- No true in-window backdrop blur; the existing translucent "glass" treatment remains a UI-only style.
- No packaging, installer, license selection, or Windows-specific work in this phase.
- No default execution of window-behavior tests; they compile and remain manually verified.

## Approaches Considered

### A. Keep per-entry dialog construction and patch each caller

Lowest diff, but every new preference has to be threaded through every caller again. The F6 defect exists precisely because of this. Rejected.

### B. Single preferences coordinator with narrow handler callbacks

`PreferencesCoordinator` builds one dialog from getters/setters supplied by `MainWindow`. Settings and F6 both call it. The coordinator owns no storage and no playback logic; it only wires the dialog to the current values and change handlers. Testable without a visible window. **Chosen.**

### C. Inject the full `SessionManager`, `FolderMenu`, and `AppearanceMenu` into every menu widget

Would centralize state but couples small UI components to storage and desktop coordination. More dependencies and harder tests. Rejected in favor of B's narrow callbacks.

## Interaction Model

Defaults preserve the current experience except for the new convenience actions:

- Single left click on video: play/pause (enabled by default).
- Double left click: fullscreen toggle (enabled by default; existing behavior).
- Middle click: fullscreen toggle (disabled by default).
- Wheel over video: volume by the configured step (enabled by default).
- Wheel over the timeline: seek by the configured seek step (enabled by default).
- Single-click detection waits for the double-click interval so a double click does not also pause.

The policy helpers are pure functions and are tested without OpenGL or a desktop session.

## Persistence

Interaction preferences live in the existing session INI under `interaction/*`, as optional schema-1 fields. Missing or malformed values fall back independently. The wheel step is clamped to 1–20. `--no-config` keeps changes in memory only; the existing `SessionManager` lifetime decides persistence.

## Error and Compatibility Rules

- Existing category indices remain stable; `Interaction` is appended.
- Old session files load with defaults.
- No new raw mpv calls in UI code.
- Video, HDR, decoding, and render paths are untouched.
- New visible strings are translated; the English catalog must compile.

## Verification

- Focused tests for the coordinator, interaction persistence, dialog controls, pure wheel policy, and transport wheel signal.
- Full non-window Debug suite with the standing exclusion expression.
- Relevant ASan/UBSan tests for session and UI-policy changes, with leak detection disabled only for documented platform library exits.
- `code-size`, changed-file `clang-format --dry-run --Werror`, and `git diff --check`.
- A final whole-branch review before the last documentation commit.

## Open Decisions Settled by the Implementer

- Time-mode toggle is session-only, not persisted, because it is a transient reading preference.
- The timeline wheel never falls through to QSlider's hidden one-step seek; disabled means no seek.
- The middle-click action defaults off to avoid surprising users.
- The line-count grace is 525 for ordinary files; the two registered 800-line exceptions remain unchanged.
