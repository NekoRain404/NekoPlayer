# Playlist and Discovery Design

## Intent

The player now has a stable playback core, unified preferences, configurable interaction, a persisted playlist, and a Linux install layout. This phase improves daily discoverability and queue control: find an item quickly, learn the shortcuts, stop cleanly at the current item, and present a recognizable application icon.

## Goals

- Add an in-playlist search field that filters by the cached display name, without disturbing the source queue, the playing row, or drag/drop when the filter is cleared.
- Add a keyboard-shortcut reference dialog reachable from a conventional Help menu (F1), built from the live menu actions so it always matches the current shortcuts.
- Add a session-only “stop after current” mode to the playback-mode menu; natural EOF stops the queue and disarms the mode.
- Add a runtime-painted application/window icon that matches the installed desktop icon and needs no new image-format dependency.
- Preserve the existing Qt Widgets/libmpv architecture, Wayland target, session schema, and 500/550/800 line policy.

## Non-Goals

- No search over media contents, subtitles, or the filesystem; only queued source display names.
- No playlist multi-selection or bulk editing in this phase.
- No persistence of search text, stop-after-current, or the window icon.
- No CPack/distro packaging, license selection, HDR/GPU/audio acceptance, or window-behavior test execution.

## Approaches and Decisions

### Playlist search

Options: a permanent `QSortFilterProxyModel`, a temporary proxy only while filtering, or manual row hiding. A permanent proxy complicates InternalMove drag/drop and current-row identity. Manual row hiding is not supported by `QListView`. **Chosen:** construct a proxy with the playlist model as source, but install it on the view only while the search text is non-empty. While filtering, drag/drop and row-edit actions are disabled; the playing row remains bold if visible. Clearing the filter restores the source model, drag/drop, and the source current row. Activation and single-click requests map proxy rows back to source rows; a persistent source index keeps the pending click stable across filter changes.

### Shortcut reference

Options: a hand-maintained table, a generated table from `QAction`s, or per-menu Help text. A hand-maintained table drifts. **Chosen:** build a read-only tree from the live `QMenuBar` at dialog-open time, deduplicating shared actions, showing action text, native shortcut text, and whether the shortcut is global or requires the playlist to have focus. A Help menu hosts the F1 action and the existing About action.

### Stop after current

Options: a queue flag checked at EOF, a one-shot timer, or a special repeat mode. A flag is the simplest and composes with repeat/shuffle for manual navigation. **Chosen:** `PlaylistController::stopAfterCurrent` is a session-only boolean. Enabling it disables native file looping so EOF is observable; natural EOF disarms it and finishes the queue. Explicit stop and clear also disarm it. Manual next/previous still work.

### Application icon

Options: embed the SVG and depend on Qt SVG, install only the desktop icon, or paint a QIcon at runtime. **Chosen:** a small runtime painter generates multi-size pixmaps matching the SVG mark; this avoids a new plugin/dependency and works under Qt Widgets. The installed SVG remains the desktop entry icon.

## Behavior and Compatibility

- Search uses case-insensitive substring matching on the cached display name; clearing restores the full queue and the source current row.
- Search text is session-only and does not modify the queue or session file.
- Shortcut dialog is modal, read-only, built from current actions; it shows only actions with at least one shortcut.
- Stop-after-current is session-only; it is not written to the session schema.
- Window icon does not change playback, rendering, or the desktop entry.

## Verification

- Offscreen tests for search filtering/current mapping/activation, shortcut tree rows, stop-after-current EOF behavior with real libmpv, and icon sizes/pixels.
- Full non-window Debug suite under `LC_ALL=C`, focused sanitizer tests for the controller/UI changes.
- `code-size`, changed-file `clang-format --dry-run --Werror`, `git diff --check`, translation build.
- Window behavior tests compile only; native Wayland visual/input acceptance remains manual.
