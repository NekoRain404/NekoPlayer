# Track Language and Subtitle Auto-load Implementation Plan

**Goal:** Persist ordered preferred audio/subtitle languages and a sibling-subtitle auto-load switch, applied through native mpv properties/options.

**Spec:** `docs/superpowers/specs/2026-10-25-track-language-design.md`

**Baseline:** `feat/first-playable` at `f860bd8`, after the playlist-organization phase.

## Global Constraints

- C++20 / Qt 6 Widgets / libmpv; single engine; UI must not issue raw mpv commands.
- Ordinary files target 500 lines; hard guard 550; `mainwindow.cpp` / `window_test.cpp` keep 800.
- Session schema 1 with optional fields; Wayland only; window tests compile only.
- `LC_ALL=C LANG=C` for ctest; NekoRain-only commits; preserve `docs/skills.md`.
- No polling or frame copies; language selection remains native mpv.

## Task 1: Language preferences value and persistence

**Files:** `src/session/sessionstate.h`, `sessionstore.cpp`, `sessionmanager.{h,cpp}`, new `tests/language_preferences_test.cpp` (`language-preferences` target), `tests/session_tests.cmake`.

**Behavior:**
- `LanguagePreferences { QStringList audioLanguages; QStringList subtitleLanguages; bool autoLoadExternalSubtitles = false; }` in the session state, with `normalized()` trimming tokens, dropping empties, and capping each list at 16.
- Optional schema-1 keys `tracks/preferredAudioLanguages`, `tracks/preferredSubtitleLanguages` (QStringList), and `subtitles/autoLoadExternal` (bool, default false). Missing/malformed values fall back independently.
- `SessionManager` getter/setter with normalization; Task 1 stores/loads/saves the value only (no controller calls yet).
- No schema version bump, no per-media track-memory change.

**Tests:** defaults; round-trip; malformed/empty-token/overlong lists; independent fallback; SessionManager round-trip. New target in `tests/session_tests.cmake`; do not grow `tests/session_test.cpp`.

## Task 2: Controller application

**Files:** `src/playback/mpvcontroller*.{h,cpp}` (new small `mpvcontroller_languages.cpp`), `src/session/sessionmanager.cpp` (apply loaded/set preferences to the controller), `CMakeLists.txt`, a new `tests/test_media_fixtures.cmake` fixture `test_language_media` (video + jpn/eng audio + zho/eng subtitles with language metadata), and `tests/track_language_test.cpp` (`track-language` target) in `tests/playback_tests.cmake`.

**Behavior:**
- `MpvController::setLanguagePreferences(const QStringList &audio, const QStringList &subtitle)` stores normalized lists and sets mpv `alang`/`slang` through the existing async command helper. The client is created synchronously in the controller constructor, so no readiness queue is needed; apply on every preference change.
- Property values are comma-joined tokens; an empty list sets an empty string (mpv default).
- `MpvController::setAutoLoadExternalSubtitles(bool)` stores the intent and sets the mpv `sub-auto` property to `exact` when enabled and `no` when disabled, so normal loads pick it up without touching their option strings. Recovery loads keep their explicit `sub-auto=no` because the saved external track set is restored; document that precedence.
- `SessionManager::restore` applies the loaded language preferences to the controller before `restoreFiles`/media load; `SessionManager::setLanguagePreferences` also applies immediately.
- Explicit per-media track memory is applied after `fileLoaded` as today and wins over language defaults.
- No polling; no frame copies.

**Tests:** real libmpv with the new language fixture: `alang=eng` selects English audio when no per-media memory; explicit aid still wins; `slang=eng` selects the English subtitle over the container-preferred Chinese one; sibling `.srt` auto-load on/off in a temporary directory; recovery keeps explicit external restoration without duplicates. Keep timeouts/waits consistent with `tracks_test` and use `LC_ALL=C`.

## Task 3: Preferences UI and translations

**Files:** `src/ui/preferencesdialog.{h,cpp}`, `preferencescoordinator.{h,cpp}`, `src/ui/windowactions.cpp` (coordinator handler lambdas, which is at 539/550 — keep the addition minimal), dialog/coordinator tests, `translations/NekoPlayer_en.ts`.

**Behavior:**
- Audio page: `首选音轨语言：` QLineEdit, object name `preferredAudioLanguagesEdit`, placeholder `例如：jpn,eng`, accessible name.
- Subtitle page: `首选字幕语言：` QLineEdit (`preferredSubtitleLanguagesEdit`) and `自动加载同名字幕` QCheckBox (`autoLoadExternalSubtitlesCheck`).
- `PreferencesDialog` takes `LanguagePreferences languages = {}` at the end of its constructor, stores a normalized baseline, exposes `languagePreferencesChanged(LanguagePreferences)`, and applies through `player_.setLanguagePreferences(...)`/`setAutoLoadExternalSubtitles(...)` on Apply/OK when changed. Include language changes in `updateApplyButton`.
- Restore Defaults on the Audio page clears the audio language field; on the Subtitles page clears the subtitle field and unchecks auto-load alongside the existing subtitle defaults.
- `PreferencesCoordinator::Handlers` gains `language`/`setLanguage`; `MainWindow` supplies the SessionManager-backed getter/setter (with session-less fallback) in the coordinator initializer. Persistence goes through `SessionManager::setLanguagePreferences`.
- Retranslate labels; add English catalog entries.

**Tests:** initial values, Apply payload, Restore Defaults, coordinator round-trip, accessible names.

## Task 4: Verification, documentation, and final review

- Full non-window suite, focused sanitizers, code-size, formatting, diff check, translations, README/architecture/verification/plan updates, whole-branch review, one fix wave.

**Commit:** `feat(tracks): add language and subtitle auto-load preferences`, `feat(playback): apply track language preferences`, `feat(ui): expose track language preferences`, then `docs: record track language verification`.
