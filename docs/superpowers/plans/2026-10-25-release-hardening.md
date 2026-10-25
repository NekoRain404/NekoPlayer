# NekoPlayer Release-Hardening Plan

**Goal:** Remove dead production code, fix a narrow-width transport defect, remove a playlist paint cost, add a custom seek interval, and make the build installable.

**Spec:** `docs/superpowers/specs/2026-10-25-release-hardening-design.md`

**Baseline:** `feat/first-playable` at `7357e35`.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; single engine; UI must not issue raw mpv commands.
- Ordinary handwritten files target 500 lines; hard guard 550; `src/ui/mainwindow.cpp` and `tests/window_test.cpp` keep 800.
- Wayland is the only desktop target. Window-behavior tests compile only; do not run them.
- Use `LC_ALL=C LANG=C` for ctest; the host locale is non-C and mpv rejects playback.
- Commits use only `NekoRain <yukathleenlxy@gmail.com>`; no co-author trailers.
- Preserve user-owned `docs/skills.md`; never stage it.
- `references/` is read-only.
- No playback, rendering, HDR, decoding, polling, or frame-copy changes.
- Use subagents for implementation and review; keep the controller context small.

## Phase Status

- [x] Task 1: Remove the legacy settings dialogs
- [x] Task 2: Responsive transport time width
- [x] Task 3: Playlist display-name cache
- [x] Task 4: Custom seek interval
- [x] Task 5: Install rules and desktop integration
- [x] Task 6: Verification and documentation
- [x] Task 6: Whole-branch review (2026-10-25; one fix wave, scoped re-review clean)
- [ ] Optional: native Wayland real-display acceptance

---

## Task 1: Remove the legacy settings dialogs

**Files:**
- Delete `src/ui/playbacksettingsdialog.h/.cpp`
- Delete `src/ui/subtitlesettingsdialog.h/.cpp`
- Modify `CMakeLists.txt`
- Modify `tests/window_test.cpp`
- Modify `tests/subtitle_window_test.cpp`
- Modify `docs/architecture.md` if it still lists the classes.

**Behavior:**
- Migrate `playbackSettingsApplyAndCancelPreserveIntent` and the subtitle draft/apply/cancel coverage to `PreferencesDialog` with `Category::Playback` / `Category::Subtitles`.
- Preserve the assertions: draft editing, Apply, Cancel rollback, Restore Defaults, actual decoder label, and the shared subtitle display actions.
- Delete the old classes and their CMake sources; do not keep unused headers.
- Keep object names used by the migrated tests (`hardwareDecodingChoice`, `actualDecoder`, `preferencesButtons`, subtitle controls).

**Verification:**
- `cmake --build --preset debug --target window_test subtitle_window_test preferences_dialog_test -j4`
- `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(preferences-dialog|code-size)$' --output-on-failure`
- Compile-only window targets; do not run them.
- Commit as `refactor(ui): remove legacy settings dialogs`.

## Task 2: Responsive transport time width

**Files:**
- Modify `src/ui/transportbarmetrics.h`
- Modify `tests/transport_bar_test.cpp`

**Behavior:**
- `forWidth(width)`: `width < 560` hides the time label; `560 <= width < 640` sets `buttonExtent = 32`, `volumeWidth = 76`, `timeWidth = 155`; `>= 640` keeps the existing defaults.
- At 560 the row is 7*32 + 48 + 18 + 24 + 155 + 76 = 545 px, so it fits.
- `minimumRowWidth` must remain <= width in the 560–639 band.
- Add tests at 480, 559, 560, 639, and 640, plus a widget test that the time label is hidden at 480 and visible at 560.

**Verification:**
- `cmake --build --preset debug --target transport_bar_test -j4`
- `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(transport-bar|code-size)$' --output-on-failure`
- Commit as `fix(ui): keep the transport time readable at narrow widths`.

## Task 3: Playlist display-name cache

**Files:**
- Modify `src/playback/playlistmodel.h`
- Modify `src/playback/playlistmodel.cpp`
- Modify `tests/playlist_test.cpp` or `tests/playlist_edit_test.cpp`

**Behavior:**
- Add a parallel `QStringList displayNames_` maintained on `addFiles`, `removeAt`, `moveRows`, and `clear`.
- `data(Qt::DisplayRole)` returns the cached name; `PathRole`, `ToolTipRole`, `CurrentRole`, drag/drop, and current-index identity stay unchanged.
- `paths()` returns `const QStringList &`; update callers if the compiler requires it.
- Add a focused test that display names follow add/move/remove/clear and that duplicate paths keep independent rows.

**Verification:**
- `cmake --build --preset debug --target playlist_test playlist_edit_test -j4`
- `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(playlist|playlist-edit|code-size)$' --output-on-failure`
- Commit as `perf(playback): cache playlist display names`.

## Task 4: Custom seek interval

**Files:**
- Modify `src/session/sessionstate.h` or `sessionstore.cpp`
- Modify `src/ui/preferencesdialog.h/.cpp`
- Modify `tests/session_test.cpp`
- Modify `tests/preferences_dialog_test.cpp`
- Modify `translations/NekoPlayer_en.ts`

**Behavior:**
- Replace the 5/10/30 combo with a `QSpinBox` named `seekStepChoice`, range 1–120, suffix ` 秒`, default 5.
- Session load accepts 1–120; malformed/out-of-range values fall back to 5; save preserves the validated value.
- The action labels and timeline wheel continue to use the shared `MainWindow::seekStepSeconds()` helper.
- Add tests for 1, 7, 120, 0, 121, and malformed values; add a dialog Apply signal test.
- Add English translations for any new visible string.

**Verification:**
- `cmake --build --preset debug --target session_test preferences_dialog_test preferences_coordinator_test -j4`
- `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(session|preferences-dialog|preferences-coordinator|code-size)$' --output-on-failure`
- Commit as `feat(settings): allow a custom seek interval`.

## Task 5: Install rules and desktop integration

**Files:**
- Modify `CMakeLists.txt`
- Modify `src/main.cpp`
- Create `data/nekoplayer.desktop`
- Create `data/nekoplayer.svg`
- Modify `README.md` (install section only)

**Behavior:**
- Use GNUInstallDirs; install `NekoPlayer` to `${CMAKE_INSTALL_BINDIR}`.
- Install the desktop file to `${CMAKE_INSTALL_DATADIR}/applications/nekoplayer.desktop`.
- Install the icon to `${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps/nekoplayer.svg`.
- `QGuiApplication::setDesktopFileName("nekoplayer")` in `main.cpp`.
- Desktop entry: `Type=Application`, `Name=NekoPlayer`, `Exec=NekoPlayer %F`, `Icon=nekoplayer`, `Terminal=false`, `Categories=AudioVideo;Video;Player;`, common MIME types.
- SVG must be original, simple, and legible at 16–256 px; no copied upstream artwork.
- Document `cmake --install build/debug --prefix ~/.local`.

**Verification:**
- Configure/build, then `cmake --install build/debug --prefix /tmp/neko-install` and confirm the binary, desktop file, and icon paths.
- `desktop-file-validate` if available; otherwise validate with `grep`/manual checks.
- Commit as `build: install the player and desktop entry`.

## Task 6: Verification, documentation, and final review

**Files:**
- Modify `README.md`
- Modify `docs/architecture.md`
- Modify `docs/verification.md`
- Update the plan checkboxes.

**Steps:**
- Run the full non-window suite under `LC_ALL=C`, focused sanitizer tests, `code-size`, formatting, and `git diff --check`.
- Record exact results and the Wayland/HDR/GPU/audio/packaging caveats.
- Dispatch a fresh whole-branch reviewer; fix Critical/Important findings in one wave and run one scoped re-review.
- Commit docs as `docs: record release-hardening verification`.

## Verification Checklist

- [x] Full Debug build succeeds; window tests compile only.
- [x] Non-window Debug suite passes.
- [x] Focused sanitizer tests pass with the documented leak-detection caveat.
- [x] `code-size`, changed-file formatting, and `git diff --check` pass.
- [x] `cmake --install` produces a binary, desktop entry, and icon.
- [x] No raw mpv commands in UI code; no polling or frame copies.
