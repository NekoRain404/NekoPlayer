# Per-Media Track Preferences Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist and restore local media audio/subtitle choices, including externally loaded subtitle files, without allowing stale asynchronous work to affect another media item.

**Architecture:** A Qt Core `TrackPreferenceStore` owns versioned, atomic persistence and a 100-entry LRU-like history. `MpvController` exposes an asynchronous restore workflow using track snapshots and mpv commands. `SessionManager` owns media identity and saves observed user intent, while renderer recovery continues using its in-memory snapshot.

**Tech Stack:** C++20, Qt 6 Core/Test, libmpv client API, CMake/CTest.

**Spec:** `docs/superpowers/specs/2026-10-22-per-media-track-preferences-design.md`

## Global Constraints

- Ordinary handwritten code files must stay at or below 500 lines; existing listed coordination and desktop-integration exceptions may reach 800 lines.
- Keep playback state/commands, graphics resources, and desktop interaction in separate layers.
- Use explicit error handling, RAII, Qt parent ownership, and target-based CMake.
- Playback must continue when a preference is missing, stale, malformed, or cannot be restored.
- Do not execute window behavior tests; verify playback and persistence through headless tests.
- Normal playback remains on libmpv's GPU render path; do not add CPU frame copies.

---

### Task 1: Track Preference Store

**Files:**
- Create: `src/session/trackpreferencestore.h`
- Create: `src/session/trackpreferencestore.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/track_preference_store_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interface:** `TrackPreferenceEntry` contains normalized media path, file size, modification timestamp, last-used timestamp, optional internal audio/subtitle IDs, external subtitle path/stream identity, and explicit disabled selections. `TrackPreferenceStore(QString filePath)` provides `load()` and `save(entries)` with the same error-result pattern as `PlaybackHistoryStore`.

- [x] Write tests for round-trip, normalized-key deduplication, mismatch-relevant fingerprint fields, 100-entry retention, and unknown schema preserving the original file.
- [x] Run `track-preference-store`; initial CMake generation failed because the planned source did not exist, then implementation verification passed.
- [x] Implement the value types, path/file validation, parsing, atomic INI write, and bounded retention.
- [x] Run `track-preference-store` and the existing `playback-history-store` test.
- [ ] Commit as `feat: add per-media track preference store`.

### Task 2: Asynchronous Controller Restore

**Files:**
- Modify: `src/playback/mpvcontroller.h`
- Modify or split: `src/playback/mpvcontroller_p.h`, `src/playback/mpvcontroller_events.cpp`, `src/playback/mpvcontroller_subtitles.cpp`
- Create: `src/playback/trackrestore.*` only if it keeps files below the size limit and avoids widening UI responsibilities.
- Create or modify: `tests/subtitles_test.cpp` or a focused controller test.

**Interface:** Controller restore accepts a saved track preference and request generation, loads only recorded external subtitle paths, then resolves audio/subtitle tracks from fresh `MediaTrack` snapshots. User selection, new load, stop, or failure invalidates pending restore work.

- [ ] Add headless libmpv tests demonstrating restoration of an alternate audio track, embedded subtitle, external subtitle, disabled track, and external subtitle stream identity.
- [ ] Run the focused test and confirm each new expectation fails against current controller behavior.
- [ ] Implement asynchronous restore using the existing mpv event queue and typed command paths; preserve renderer recovery behavior.
- [ ] Test delayed/failed subtitle load and stale request invalidation after media switch and user choice.
- [ ] Run focused controller tests and `tracks`, `subtitles`, and `playback` regressions.
- [ ] Commit as `feat: restore media track choices asynchronously`.

### Task 3: Session Lifecycle Persistence

**Files:**
- Modify: `src/session/sessionmanager.h`
- Modify: `src/session/sessionmanager.cpp`
- Create: `tests/track_preference_session_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interface:** `SessionManager` owns `TrackPreferenceStore`, captures observed track snapshots for the active local regular file, checks file fingerprints at new-media open, requests controller restoration only for new media, and persists the final available snapshot on media transition or shutdown.

- [ ] Write integration tests for restart restoration, changed-file fingerprint rejection, missing-file tolerance, repeated queue paths, and user choice during pending restoration.
- [ ] Run `track-preference-session`; confirm the integration expectations fail before implementation.
- [ ] Implement normalized media identity and snapshot capture without subscribing to progress or frame callbacks.
- [ ] Run the new session integration test plus `playback-history-session`, `session`, `tracks`, and `subtitles` tests.
- [ ] Commit as `feat: persist track choices across sessions`.

### Task 4: Verification and Documentation

**Files:**
- Modify: `docs/architecture.md`
- Modify: `docs/verification.md`
- Modify: `README.md`

- [ ] Build the full Debug preset.
- [ ] Run CTest excluding window/desktop interaction tests; report the exact excluded set and result count.
- [ ] Build and run focused store/session/playback tests with ASan/UBSan.
- [ ] Run `git diff --check`, formatting checks, and the 500/800-line code-size check.
- [ ] Update docs with persistence behavior, known limitations, and exact verification coverage.
- [ ] Commit as `docs: document per-media track preferences`.

## Spec Coverage

Store schema, path/fingerprint validation, atomic writes, bounded retention, malformed-file preservation, asynchronous loading and selection, user intent cancellation, generation isolation, renderer recovery coexistence, session capture, restart behavior, loss/error behavior, headless verification, sanitizers, documentation, and code-size constraints are covered by Tasks 1-4.
