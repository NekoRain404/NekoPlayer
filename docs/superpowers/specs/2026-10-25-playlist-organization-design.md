# Playlist Organization Design

## Intent

The playlist now supports search, reorder, persistence, loop/shuffle, and
stop-after-current, but everyday queue maintenance still requires deleting one
row at a time and offers no way to clean missing files or sort a large queue.
This phase makes the list feel complete for a PotPlayer-like desktop player
without changing playback semantics.

## Goals

- Multi-select rows with Ctrl/Shift and remove all selected rows in one action.
- Remove every missing local file after an explicit count confirmation.
- Stable sort by title or path, ascending or descending, preserving the current
  item's identity and continuing playback uninterrupted.
- Show a clear empty-state hint in the playlist instead of a blank pane.
- Keep search, drag reorder, current-row editing, and session persistence
  working as they do today.

## Non-Goals

- No duplicate detection or automatic removal; duplicate paths remain distinct.
- No multi-row drag; dragging stays a single-entry operation.
- No sort by duration or media metadata (would require probing files).
- No undo stack, no sort-mode persistence, no playlist file format change.
- No playback/render or libmpv changes.

## Decisions

- Selection uses `ExtendedSelection`. Remove is enabled for any non-empty
  selection; move up/down only for a single selected row. Click-to-play fires
  only for an unmodified click: Ctrl/Shift/Meta clicks change the selection and
  cancel any pending single-click activation, so extending a selection never
  starts playback.
- Bulk removal goes through a controller API with batch semantics: playback
  stops only when the current row is removed, and the replacement current row
  follows the existing single-remove rule (next surviving row, else previous).
  The model method is named `removeRowsAt` so it cannot be confused with Qt's
  row/count removal API.
- Missing-file detection reuses the controller's local-file playability rule
  (network URLs are never considered missing). The playlist menu offers one
  count-confirmed cleanup action; it runs synchronously on the GUI thread with a
  wait cursor, and no playback reload occurs.
- Sorting rewrites the queue order in `PlaylistModel` using a stable,
  case-insensitive natural collation on display names, tie-broken by path. The
  existing numeric-aware collation in `mediafolderscan.cpp` is extracted into a
  small shared helper used by both folder scanning and playlist sorting.
  Persistent indexes are remapped, so the playing item and view selection
  follow their entries. Controller failure history is cleared and shuffle order
  is rebuilt around the current row, because both are run-scoped row histories.
- Sort actions live in a `播放列表 → 排序` submenu; one `playlistEmptyHint` label
  lives in `PlaylistPane`: it shows `没有匹配项` when a non-empty search matches
  nothing, `播放列表为空` with a short open/drop hint when the queue is empty, and
  is hidden otherwise.

## Verification

Model unit tests, real-libmpv controller tests, offscreen pane tests, compile-only
window assertions, full non-window suite, focused sanitizers, code-size,
formatting, diff check, translations, docs.
