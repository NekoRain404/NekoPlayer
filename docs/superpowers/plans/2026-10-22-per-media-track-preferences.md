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
- [x] Commit as `feat: add per-media track preference store` (`7851985`).

### Task 2: Asynchronous Controller Restore

**Files:**
- Modify: `src/playback/mpvcontroller.h`
- Modify or split: `src/playback/mpvcontroller_p.h`, `src/playback/mpvcontroller_events.cpp`, `src/playback/mpvcontroller_subtitles.cpp`
- Create: `src/playback/trackrestore.*` only if it keeps files below the size limit and avoids widening UI responsibilities.
- Create or modify: `tests/subtitles_test.cpp` or a focused controller test.

**Interface:** Controller restore accepts a saved track preference and request generation, loads only recorded external subtitle paths, then resolves audio/subtitle tracks from fresh `MediaTrack` snapshots. User selection, new load, stop, or failure invalidates pending restore work.

- [x] Add headless libmpv tests demonstrating restoration of an alternate audio track, embedded subtitle, external subtitle, disabled track, and external subtitle stream identity.
- [x] Run focused regressions during implementation; record reproduced failures and fixes in the execution record below. A separate initial failure for every conceptual expectation was not recorded.
- [x] Implement asynchronous restore using the existing mpv event queue and typed command paths; preserve renderer recovery behavior.
- [x] Test failed subtitle load and stale request invalidation after media switch, user choice, stop, and reentrant signal callbacks.
- [x] Run focused controller tests and `tracks`, `subtitles`, and `playback` regressions.
- [x] Include controller restoration in the combined Tasks 2–4 stage commit, `feat: persist and restore per-media track choices`.

### Task 3: Session Lifecycle Persistence

**Files:**
- Modify: `src/session/sessionmanager.h`
- Modify: `src/session/sessionmanager.cpp`
- Create: `tests/track_preference_session_test.cpp`
- Modify: `tests/CMakeLists.txt`

**Interface:** `SessionManager` owns `TrackPreferenceStore`, captures observed track snapshots for the active local regular file, checks file fingerprints at new-media open, requests controller restoration only for new media, and persists the final available snapshot on media transition or shutdown.

- [x] Write integration tests for restart restoration, same-file reopen, changed-file fingerprint rejection, missing-file tolerance, renderer recovery, and corrupt configuration; controller tests cover user choice during pending restoration.
- [x] Run `track-preference-session`; reproduce media-switch intent leaking from the previous path, then verify the lifecycle isolation fix.
- [x] Implement normalized media identity and snapshot capture without subscribing to progress or frame callbacks.
- [x] Run the new session integration test plus `playback-history-session`, `session`, `tracks`, and `subtitles` tests.
- [x] Include session persistence in the combined Tasks 2–4 stage commit.

### Task 4: Verification and Documentation

**Files:**
- Modify: `docs/architecture.md`
- Modify: `docs/verification.md`
- Modify: `README.md`

- [x] Build the full Debug preset.
- [x] Run CTest excluding window/desktop interaction tests; report the exact excluded set and result count in `docs/verification.md`.
- [x] Build the full sanitizer preset and run the non-window store/session/playback collection with ASan/UBSan.
- [x] Run `git diff --check`, formatting checks, and the 500/800-line code-size check.
- [x] Update docs with persistence behavior, known limitations, and exact verification coverage.
- [x] Include documentation in the combined Tasks 2–4 stage commit.

## Execution Record

Task 1 was committed independently. Controller restoration and session capture were developed and reviewed together because completion, cancellation, and snapshot publication form one lifecycle contract; Tasks 2–4 ship as one subsequent stage commit.

The final implementation awaits each selection command reply and queries a fresh final snapshot before completing. Ordinary opens reset `aid`/`sid` to automatic defaults before applying the media record. Renderer recovery uses its existing in-memory snapshot and suppresses intermediate persistence. The file fingerprint is captured at load, so modifying a playing file cannot attach the old selection to its replacement.

Review added corruption tests for invalid presence flags and external-stream values, stale-history pruning, and reentrant cancellation. Broad verification exposed an existing metadata refresh gap when an audio output was reused, a reply-queue saturation test consuming a synthesized property notification instead of a reply, and a subtitle-drop test requesting pause before `FILE_LOADED`. The metadata path now refreshes on playback restart; the two tests now wait for their actual prerequisites. Exact final commands, results, and desktop limitations are recorded in `docs/verification.md`.

## Spec Coverage

Store schema, path/fingerprint validation, atomic writes, bounded retention, malformed-file preservation, asynchronous loading and selection, user intent cancellation, generation isolation, renderer recovery coexistence, session capture, restart behavior, loss/error behavior, headless verification, sanitizers, documentation, and code-size constraints are covered by Tasks 1-4.
