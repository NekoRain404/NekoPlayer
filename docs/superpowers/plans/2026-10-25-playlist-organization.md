# Playlist Organization Implementation Plan

**Goal:** Add multi-select bulk removal, missing-file cleanup, queue sorting, and
an empty-state hint to the playlist.

**Spec:** `docs/superpowers/specs/2026-10-25-playlist-organization-design.md`

**Baseline:** `feat/first-playable` at `e23ff29`, after the settings-and-window phase.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; no playback/render changes; UI must not issue raw mpv commands.
- Ordinary files target 500 lines; hard guard 550; `mainwindow.cpp` / `window_test.cpp` keep 800.
- Wayland only; window tests compile only. `LC_ALL=C LANG=C` for ctest.
- Commits only under NekoRain; no co-author trailers; `docs/skills.md` is user-owned.
- Use subagents for implementation and review.

## Task 1: Model batch removal and sorting

**Files:** `src/playback/playlistmodel.{h,cpp}`, new `src/playback/naturalcompare.{h,cpp}` added to `neko_playback`, `src/input/mediafolderscan.cpp`, `tests/playlist_edit_test.cpp`, `tests/playback_tests.cmake` only if a target change is needed.

**Behavior:**
- Add `int removeRowsAt(const QList<int> &rows)` (deliberately not named like Qt's
  virtual `removeRows`) that removes unique valid rows with
  per-row `beginRemoveRows`/`endRemoveRows`, emits `currentIndexChanged` at most
  once, and replaces a removed current row with the next surviving row (else the
  previous); all removed clears the current row.
- Extract the numeric-aware collation from `src/input/mediafolderscan.cpp`
  into a shared helper and reuse it for folder scans and title sorting.
- Add `enum class SortKey { Title, Path }` and
  `bool sortEntries(SortKey key, Qt::SortOrder order)` using stable natural,
  case-insensitive title comparison, path tiebreak, `layoutAboutToBeChanged` /
  `layoutChanged`, and `changePersistentIndexList`.
- Keep the current row attached to its entry; emit `currentIndexChanged` when
  its row changes.

**Tests:** duplicates stay distinct; removing current selects neighbor; removing
all clears; sort order, stability, current identity, persistent index remap;
filtered model updates are covered in Task 3.

## Task 2: Controller bulk and missing-file operations

**Files:** `src/playback/playlistcontroller.{h,cpp}`, controller tests.

**Behavior:**
- `void removeRowsAt(const QList<int> &rows)`: stop once if the current row is
  included, disarm stop-after-current, remap/clear failure history, delegate to
  the model, reset shuffle when the current row changed, emit `currentChanged`.
  `removeAt(row)` delegates to it.
- `QList<int> missingFileRows() const` and `int removeMissingFiles()` using the
  existing local playability rule; network entries are never missing.
- `void sortPlaylist(PlaylistModel::SortKey key, Qt::SortOrder order)`: clear failure
  history, sort the model, rebuild shuffle around the new current row, emit
  `currentChanged`; never reload media.
- No behavior change for file order outside these calls.

**Tests:** bulk removal keeps playback unless current is removed; current
replacement; missing detection ignores network and existing files; missing
removal of current stops; sort preserves playback and current path; shuffle
reset remains coherent.

## Task 3: Playlist pane selection and empty state

**Files:** `src/ui/playlistpane.{h,cpp}`, offscreen pane tests, translations.

**Behavior:**
- `ExtendedSelection`; remove gathers source rows from `selectedRows()`,
  removes them descending through the controller; move up/down only for exactly
  one selected row and otherwise disabled.
- Preserve single-click activation and double-click/Enter behavior; a pending
  single-click target that is removed is dropped. Ctrl/Shift/Meta clicks must
  not schedule playback; they only extend the selection.
- Add one `playlistEmptyHint` label with an accessible name. When the source
  queue is empty it shows `播放列表为空` plus the hint `打开文件或文件夹，或拖入媒体`;
  when a non-empty filter matches nothing it shows `没有匹配项`; otherwise it is
  hidden.
- Retranslate all new strings.

**Tests:** Ctrl/Shift selection enables remove; bulk remove updates queue and
count; move actions disabled for multi-selection; single-click still works;
empty hint toggles on clear/add; a non-matching filter shows `没有匹配项`, and
clearing the filter restores the empty-queue text or hides the label as
appropriate.

## Task 4: Sort menu and integration

**Files:** create `src/ui/playlistmaintenancemenu.{h,cpp}` (owns the sort submenu and the cleanup action), `CMakeLists.txt`, `src/ui/windowactions.cpp`, compile-only window tests, `tests/ui_tests.cmake`, translations.

**Behavior:**
- `播放列表 → 排序` submenu with four actions: 标题升序, 标题降序, 路径升序,
  路径降序; stable object names; no shortcuts; retranslate on `LanguageChange`.
- Actions call `PlaylistController::sortPlaylist`; filtering stays valid because
  the proxy follows source layout changes; current selection follows identity.
- Add `移除缺失文件…` to the playlist menu: scan `missingFileRows()`, show a
  count confirmation, remove through the controller with a wait cursor, and
  report `已移除 N 个缺失文件` or `没有缺失文件` through existing feedback. Cancel
  does nothing. Network entries are never proposed for removal.
- Add an offscreen `playlist-maintenance-menu` test for the sort actions and
  object names; the modal missing-file confirmation stays compile-only. Add
  compile-only window assertions that the playlist menu contains the sort
  submenu and cleanup action.

## Task 5: Verification and documentation

- Full non-window suite, focused sanitizers, code-size, formatting, diff check,
  translations, install layout, docs update, whole-branch review, one fix wave.

## Verification Checklist

- [x] Full Debug build succeeds; window tests compile only.
- [x] Non-window Debug suite passes under `LC_ALL=C LANG=C`.
- [x] Focused sanitizer targets pass with the documented leak-detection caveat.
- [x] `code-size`, changed-file formatting, and `git diff --check` pass.
- [x] No raw mpv commands in UI code; no new polling or frame copies.
- [x] README/architecture/verification/plan updated; whole-branch review clean.
      The final review findings were fixed at `832fbe0` and the scoped re-review
      is clean.

## Status (2026-10-25)

Code at `832fbe0`; commits `35961c2`, `cce15f8`, `8cf71e3`, `acc623e`,
`832fbe0` (final review fixes).

- [x] Task 1: model batch removal, stable natural title/path sorting, shared collation.
- [x] Task 2: controller bulk removal, missing-file scan/removal, sort.
- [x] Task 3: pane `ExtendedSelection`, modifier-safe click-to-play, empty-state hint.
- [x] Task 4: playlist sort submenu and count-confirmed missing-file cleanup.
- [x] Task 5: Debug build; non-window suite 62/62 in 60.44 s; focused selection 5/5 in 7.27 s; focused sanitizer 4/4 in 7.70 s with leak detection disabled for documented platform-library exit allocations; `code-size` 1/1 with advisories for `windowactions.cpp` (537) and `ui_surface_test.cpp` (550); 19 changed C++ files formatting-clean; `git diff --check` clean excluding `docs/skills.md`; 309/309 translations finished; three-file install layout; no raw mpv use in `src/ui/`; all commits authored by NekoRain with no co-author trailers.
- [x] Final review fix wave at `832fbe0`: revalidated missing-file cleanup after confirmation with the actual count; sort cancels a pending folder import; press-time modifier cancellation and modifier clearing; empty-queue hint precedence; failure history cleared only after a real sort; wait cursors for sort/scan/removal; three new `playlist-pane` tests. Re-verified: non-window **62/62** in **61.13 s**; focused **6/6** in **7.93 s**; focused sanitizers **3/3** in **6.97 s** with leak detection disabled for the documented platform-library caveat; `code-size` 1/1 with advisories for `windowactions.cpp` (539), `playlist_pane_test.cpp` (502) and `ui_surface_test.cpp` (550); 5 changed C++ files formatting-clean; `git diff --check` clean excluding `docs/skills.md`; 309/309 translations finished; three-file install layout.
- [ ] Native Wayland visual/keyboard/pointer acceptance was not performed; window tests remain compile-only.
