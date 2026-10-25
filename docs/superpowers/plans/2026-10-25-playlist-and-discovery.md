# Playlist and Discovery Implementation Plan

**Goal:** Add playlist search, a live shortcut reference, stop-after-current, and an application icon.

**Spec:** `docs/superpowers/specs/2026-10-25-playlist-and-discovery-design.md`

**Baseline:** `feat/first-playable` at `7e098ad`.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; single engine; UI must not issue raw mpv commands.
- Ordinary files target 500 lines; hard guard 550; `src/ui/mainwindow.cpp` and `tests/window_test.cpp` keep 800.
- Wayland only. Window-behavior tests compile only; do not run them.
- Use `LC_ALL=C LANG=C` for ctest.
- Commits use only `NekoRain <yukathleenlxy@gmail.com>`; no co-author trailers.
- Preserve user-owned `docs/skills.md`; never stage it.
- `references/` is read-only.
- No playback, rendering, HDR, decoding, polling, or frame-copy changes beyond the explicit stop-after-current EOF handling.
- Use subagents for implementation and review; keep the controller context small.

---

## Task 1: Playlist search and filtering

**Files:**
- Modify `src/ui/playlistpane.h`
- Modify `src/ui/playlistpane.cpp`
- Modify `src/ui/appearancestylesheet.cpp` only if the search field needs a stable object selector
- Create `tests/playlist_pane_test.cpp`
- Modify `tests/CMakeLists.txt`
- Modify `translations/NekoPlayer_en.ts`

**Interfaces / behavior:**
- Add a `QLineEdit` named `playlistSearchEdit` under the playlist header, with clear button, placeholder `搜索播放列表`, and accessible name.
- Add pane-owned search actions with `Qt::WidgetWithChildrenShortcut`: `搜索播放列表` (`QKeySequence::Find`, object name `focusPlaylistSearchAction`) focuses and selects the search field, and `清除播放列表搜索` (Escape, object name `clearPlaylistSearchAction`) clears it. Expose both through `shortcutActions()` for the shortcut reference.
- Add a `QSortFilterProxyModel` as a member, source = `PlaylistModel`, case-insensitive, filter column 0, dynamic.
- While the trimmed search text is non-empty, set the view model to the proxy; disable InternalMove drag/drop and the remove/up/down editing actions. While empty, set the view back to the source model and restore drag/drop.
- Add helpers mapping a view index to the source row; use them for `activated`, single-click pending indices, and editing actions.
- `synchronizeCurrent()` maps the source current row into the proxy when filtering; if it is hidden, clear the view selection. Clearing the filter selects the source current row.
- Clear the search on a source-model reset so a replacement queue is not hidden by a stale filter.
- The item count shows `"%1 / %2 项"` while filtering and `"%1 项"` otherwise.
- Do not modify the queue, current row, playback, or drag/drop semantics when the filter is empty.

**Tests:**
- Add an offscreen `playlist-pane` test target linking `neko_ui`/`Qt6::Test`.
- Cover: filter narrows rows, activation maps to the source row, editing actions disabled while filtering, Escape/clear restores the full queue and source current row, current row hidden by filter clears selection, and the playing row remains bold when visible.
- Build all targets; window tests compile only.

**Commit:** `feat(ui): add playlist search and filtering`.

## Task 2: Keyboard shortcut reference and Help menu

**Files:**
- Create `src/ui/shortcutsdialog.h/.cpp`
- Modify `src/ui/windowactions.cpp`
- Modify `src/ui/mainwindow.h` only if a member/helper is needed
- Modify `CMakeLists.txt`
- Create `tests/shortcuts_dialog_test.cpp`
- Modify `tests/CMakeLists.txt`
- Modify `translations/NekoPlayer_en.ts`

**Interfaces / behavior:**
- `ShortcutsDialog` is a `QDialog` with a read-only `QTreeWidget` (`shortcutsTree`) and columns `操作`, `快捷键`, `范围`.
- Construction takes a `QMenuBar *` plus an optional list of extra pane-owned `QAction`s; it recursively walks menus, deduplicates shared `QAction`s, and includes only actions with non-empty shortcuts. Extra actions that are not already menu actions appear under the top-level `面板快捷键` item, pruned when empty.
- Scope is `列表聚焦` for `Qt::WidgetShortcut`, `当前面板` for `Qt::WidgetWithChildrenShortcut`, otherwise `全局`. Menu titles are top-level items.
- `MainWindow` passes `PlaylistPane::shortcutActions()` so the search shortcuts appear in the reference.
- Add a checkable-free `shortcutsAction` (`快捷键…`, F1, object name `shortcutsAction`) and a `帮助(&H)` menu after Settings containing the shortcut action and the existing About action.
- Update the `LanguageChange` menu-title array and action text path for the Help menu/action.
- The dialog is modal and built fresh on each open so it reflects the current language/shortcuts.

**Tests:**
- Add a `shortcuts-dialog` test target. Build a QMenuBar with a global action, a widget-scoped action, a no-shortcut action, and a submenu; assert the tree includes only the shortcut actions, correct scope labels, native shortcut text, and no duplicate shared action.
- Assert extra actions appear under `面板快捷键` with `当前面板`, duplicates are not repeated, and an empty extra list prunes the section.
- Compile-only window assertion for the F1 action/menu if a non-window test cannot reach it.

**Commit:** `feat(ui): add a live keyboard shortcut reference`.

## Task 3: Stop after current

**Files:**
- Modify `src/playback/playlistcontroller.h/.cpp`
- Modify `src/ui/playlistmodemenu.h/.cpp`
- Modify `tests/playlist_modes_test.cpp`; no session schema changes.

**Interfaces / behavior:**
- Add `bool stopAfterCurrent() const`, `void setStopAfterCurrent(bool)`, and `stopAfterCurrentChanged(bool)`.
- Enabling it disables native file looping (including repeat-one/all-single) so EOF is observable; disabling restores the existing loop behavior.
- On natural EOF, if armed: disarm, finish the queue, and do not advance. Explicit stop and `clear()` also disarm.
- Manual next/previous and repeat/shuffle navigation remain available.
- Add a checkable `播放完当前停止` action (`stopAfterCurrentAction`) to the playback-mode menu; checked state follows the controller; retranslate it.
- Session-only; do not write it to the session.

**Tests:**
- Real libmpv: two-item queue, arm on first, EOF stops the queue and does not advance; the mode disarms.
- Repeat-one plus armed stops instead of looping.
- Explicit stop/clear disarms.
- Manual next still advances while armed.

**Commit:** `feat(playback): add stop-after-current mode`.

## Task 4: Runtime application icon

**Files:**
- Create `src/ui/appicon.h/.cpp`
- Modify `src/main.cpp`
- Modify `CMakeLists.txt`
- Create `tests/app_icon_test.cpp`
- Modify `tests/CMakeLists.txt`

**Interfaces / behavior:**
- `QIcon appIcon()` paints the existing original mark (dark rounded cat-eared tile, accent play triangle) at 16, 24, 32, 48, 64, 128, and 256 px, with antialiasing and transparent background.
- `QApplication::setWindowIcon(appIcon())` in the GUI branch of `main.cpp` before the window is created.
- No Qt SVG/plugin dependency, no external assets, no playback/render changes.
- The installed `data/nekoplayer.svg` remains the desktop-entry icon.

**Tests:**
- Offscreen `app-icon` test: icon is non-null, contains the expected sizes, pixmaps are non-empty, the play-mark pixel differs from the tile background, and disabled/high-DPI behavior is not required.
- Compile only for window targets.

**Commit:** `feat(ui): add a runtime application icon`.

## Task 5: Verification, documentation, and final review

**Files:**
- Modify `README.md`
- Modify `docs/architecture.md`
- Modify `docs/verification.md`
- Update this plan's checkboxes.

**Steps:**
- Run the full non-window suite under `LC_ALL=C`, focused sanitizer tests, `code-size`, formatting, and `git diff --check`.
- Record exact results, install layout, translations, and the Wayland/HDR/GPU/audio/Windows caveats.
- Dispatch a fresh whole-branch reviewer; fix Critical/Important in one wave and run one scoped re-review.
- Commit docs as `docs: record playlist and discovery verification`.

## Verification Checklist

- [x] Full Debug build succeeds; window tests compile only.
- [x] Non-window Debug suite passes.
- [x] Focused sanitizer tests pass with the documented leak-detection caveat.
- [x] `code-size`, changed-file formatting, and `git diff --check` pass.
- [x] No raw mpv commands in UI code; no polling or frame copies.
