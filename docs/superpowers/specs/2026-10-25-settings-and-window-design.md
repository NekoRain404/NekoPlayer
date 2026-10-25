# Settings and Window Behavior Design

## Intent

After the playlist/discovery phase, the player needs a few finishing settings and window controls that desktop users expect: one volume step shared by keyboard and wheel, a persisted elapsed/remaining default, always-on-top with correct mini-player restoration, and startup flags for fullscreen and paused playback.

## Goals

- Use the configured volume step for both wheel and keyboard +/-; relabel the setting accordingly.
- Persist a default time-display mode (elapsed or remaining); the transport click remains a transient override.
- Add a persisted always-on-top toggle in the View menu; exiting mini-player restores the user's choice instead of forcing off.
- Add `--fullscreen` and `--start-paused` CLI options, preserving existing flags.
- Keep the session schema version 1 with optional fields; no playback/render changes.

## Non-Goals

- No playlist sorting/bulk removal in this phase.
- No CPack/distro packaging or license selection.
- No native Wayland visual acceptance claim.
- No second engine, Qt Quick, or new rendering path.

## Decisions

- Volume step: reuse `InteractionPreferences::wheelVolumeStep` for keyboard +/-; keep the existing settings key for compatibility, relabel the UI to "音量步进".
- Time display: add `interaction/showRemainingTime` (default false) and `TransportBar::setTimeShowsRemaining`; the label click is session-only.
- Always-on-top: add `window/alwaysOnTop`; mini-player entry forces on-top, exit restores the persisted value; fullscreen toggling is not allowed to overwrite it.
- CLI: `--fullscreen` enters through the controlled `MainWindow::enterFullscreen()` transition after `show()`, which captures normal geometry before fullscreen and keeps the existing WindowStateChange synchronization; `--start-paused` pauses once after the first successful `fileLoaded`.

## Verification

Focused session/dialog/transport/MainWindow compile-only tests, full non-window suite, sanitizers, code-size, formatting, diff check, translations, docs.
