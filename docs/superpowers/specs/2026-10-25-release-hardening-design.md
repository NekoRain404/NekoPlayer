# NekoPlayer Release-Hardening Design

## Intent

The first release-readiness phase unified preferences, added configurable interaction, polished the transport and playlist, and removed per-tick GUI churn. This second phase removes remaining dead production code, closes a narrow-width rendering defect, removes a per-paint cost, expands a useful setting, and makes the build installable as a desktop application.

## Goals

- Delete the production-dead `PlaybackSettingsDialog` and `SubtitleSettingsDialog`; migrate their window-test coverage to the unified `PreferencesDialog`.
- Fix the transport time label at the 480–639 px band so elapsed/remaining text is not clipped at the 560 px minimum window width.
- Cache playlist display names so painting does not recompute `MediaSource::displayName` per row per paint; keep path/identity roles unchanged.
- Replace the fixed 5/10/30 s seek-interval choices with a validated 1–120 s setting, persisted in the existing session schema.
- Add install rules, a desktop entry, and an original scalable icon so a packaged Linux build has a launcher and metadata.
- Keep the existing architecture: Qt Widgets, QOpenGLWidget + libmpv, no polling, no frame copies, UI never issues raw mpv commands.

## Non-Goals

- No Qt Quick, second engine, plugin framework, or skin system.
- No playlist search/filter proxy in this phase.
- No license selection, Windows packaging, Flatpak/AppImage, or crash recovery.
- No native Wayland visual acceptance claim; window tests remain compile-only.
- No true in-window backdrop blur.

## Approaches and Decisions

### Legacy dialogs

Options: keep and mark legacy, or delete and migrate tests. Keeping them preserves two dead constructor paths and stale documentation. Deleting them reduces code and forces the tests onto the dialog users actually see. **Chosen:** delete; migrate the window tests to `PreferencesDialog` categories Playback and Subtitles.

### Narrow transport time

Options: shrink the font, hide the label, or allocate the measured width. Shrinking harms legibility; hiding at the minimum window width loses useful information. The row fits with a 150 px label at 560 px (314 px controls + 76 px volume + 150 px time = 540). **Chosen:** below 560 px hide the time label; 560–639 px use 150 px; ≥640 px keep 155 px. The tests assert the metrics and the actual widget visibility at 560/639/640.

### Playlist display-name cache

Options: compute lazily on paint, cache on mutation, or store a display string in the model rows. Caching a parallel `QStringList` on add/remove/move/clear keeps `data()` O(1) and preserves the existing path-based identity and drag/drop. **Chosen:** parallel cache; `paths()` returns a const reference.

### Seek interval

Options: keep presets, add a custom combo entry, or use a spin box. A spin box is the clearest and validates a single range. **Chosen:** `QSpinBox` 1–120 seconds, default 5; session load clamps, malformed values fall back to 5.

### Packaging

Options: CPack, distro packaging, or install rules only. CPack/distro packaging is premature without a license and target distro. **Chosen:** GNUInstallDirs-based `install()` rules, a `.desktop` entry, and an original SVG icon; document `cmake --install`.

## Compatibility

- Session schema stays version 1; seek step remains the existing `playback/seekStepSeconds` key with a wider validated range.
- Existing object names used by tests are preserved where the dialog is migrated.
- No changes to playback commands, mpv options, decoding, HDR, or the render path.

## Verification

- Focused tests for migrated dialog coverage, transport metrics/visibility, playlist display-name cache, seek-step clamping, and packaging file presence.
- Full non-window Debug suite under `LC_ALL=C`.
- Focused ASan/UBSan for the changed session/UI tests.
- `code-size`, changed-file `clang-format --dry-run --Werror`, `git diff --check`.
- A whole-branch review before the final documentation commit.
