# Track Language and Subtitle Auto-load Design

## Intent

NekoPlayer remembers explicit per-media track choices, but a fresh file always
falls back to mpv's container-default selection and never auto-loads a sibling
subtitle. Desktop players let users state language preferences once and load
matched subtitle files automatically. This phase adds those defaults without
changing explicit per-media memory.

## Goals

- Persist ordered preferred audio and subtitle language lists.
- Persist a switch for auto-loading external subtitle files next to the media.
- Apply preferences through mpv's native `alang`/`slang` properties and
  `sub-auto` load option, so no frame/event polling is added.
- Expose both in the existing Audio and Subtitle preference pages.
- Explicit per-media track memory still overrides language defaults.

## Non-Goals

- No subtitle download/search, online metadata, or encoding-detection UI.
- No per-file or per-directory language rules; one global preference.
- No change to the per-media track-memory schema or explicit selection semantics.
- No new playback engine, render path, polling, or CPU frame copies.

## Decisions

- Language input is a comma-separated list of mpv tokens (ISO 639-1/639-2 or
  full names accepted by mpv), trimmed; empty tokens are dropped and each list
  is capped at 16 entries. Empty input means "mpv default" (empty property).
- The normalized lists live in a `LanguagePreferences` value in the session
  state and are stored as schema-1 optional `QStringList` keys
  `tracks/preferredAudioLanguages` and `tracks/preferredSubtitleLanguages`;
  `subtitles/autoLoadExternal` is an optional bool defaulting to false.
  Malformed/missing values fall back independently.
- `MpvController::setLanguagePreferences` stores the lists and sets `alang` and
  `slang` through the existing asynchronous command helper; the client is
  created synchronously in the constructor, so applying during session restore
  is safe. No renderer-reset reapplication is needed because the client and its
  properties survive renderer resets.
- Auto-load maps to `sub-auto=exact` for normal loads when enabled and `no`
  when disabled. Recovery loads keep `sub-auto=no` because the saved external
  track set is restored explicitly; this avoids duplicate external streams.
- Per-media track memory is applied after `fileLoaded` as today, so an explicit
  saved choice wins over language defaults.
- UI: Audio page gets `首选音轨语言：` (placeholder `例如：jpn,eng`); Subtitle
  page gets `首选字幕语言：` and checkbox `自动加载同名字幕`. Values are drafts
  applied on Apply/OK and reset by Restore Defaults like other prefs.

## Verification

Session round-trip/defaults/malformed, preference-dialog Apply/restore,
coordinator pass-through, real libmpv language selection with the multi-track
fixture, sibling-subtitle auto-load in a temporary directory, recovery precedence,
full non-window suite, focused sanitizers, code-size, formatting, translations,
docs.
