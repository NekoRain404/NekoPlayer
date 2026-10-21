# Verification Record

Environment: native Linux Wayland session (niri), Qt 6.12.0, system libmpv 0.41.0 and GCC 16.2.1. The verified display is one 2880x1800 panel at 1.75 scale. Window tests use the Qt Wayland platform plugin, null audio and `MALLOC_PERTURB_=128`.

Audio/subtitle track stage:

- Final results: the full Debug build succeeded; the selected Debug checks passed 5/5 in 8.14 seconds, and the ASan/UBSan integration suites passed 3/3 in 9.32 seconds with no reported sanitizer errors. Formatting and `git diff --check` also passed.
- Full Debug build includes the player and `window_test` executable. The window tests remain compile-only at the user's request; no GUI/window CTest entry is run in this stage.
- `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|tracks)$' --output-on-failure` covers code limits, backend startup and the three non-GUI integration suites.
- `cmake --build --preset sanitizers --target playback_test playlist_test tracks_test` and `ctest --test-dir build/sanitizers -R '^(playback|playlist|tracks)$' --output-on-failure` exercise the same controller paths under ASan/UBSan, including copying borrowed mpv node data. GUI sanitizer execution remains skipped.
- New FFmpeg fixtures provide two audio tracks, an embedded subtitle, and an external Matroska with two subtitle streams. Track tests verify owned metadata, typed track-ID validation, async audio/subtitle switching and disabling without media reload, UTF-8/spaced external paths, asynchronous load failure and clearing on stop/replacement.
- Recovery tests cover internal/external/disabled selection, repeated recovery without duplicated external streams, preservation of a selected second stream from a single external file, and a second reset while restoration is pending. The external-subtitle loss and overlapping-reset failures were reproduced before their fixes. An intermediate failure also confirmed that mpv can reset audio selection when reloading with a different external track layout; final selection is restored after external loading completes.
- GUI wiring assertions now check backend selections in addition to QAction checks. They compile but have not been executed, so actual menu presentation, file-dialog behavior and subtitle pixels remain unverified.

Previous debug verification (playlist navigation and completion):

- `cmake --build --preset debug` succeeded.
- `ctest --test-dir build/debug -R '^(playback|playlist)$' --output-on-failure` passed both non-GUI integration tests.
- `cmake -DNEKO_SOURCE_DIR="$PWD" -P cmake/CheckCodeSize.cmake` confirmed ordinary files are within 500 lines and the explicitly listed special files are within 800 lines.
- The playback test verifies libmpv initialization, `hwdec=auto-safe`, local-file load/seek/pause/stop, mute/volume, EOF and recovery. The playlist test also verifies reverse missing-file skipping, invalid selection rejection, EOF with a missing tail, append after EOF, append while paused and stop/replay/pause through the queue controller. Four regressions were reproduced against the previous implementation before fixes were applied.
- GUI/window tests were deliberately not run in this verification round at the user's request. `window_test` now expects local multi-file drops to append, multiple command-line files to start a queue and paused appends to update navigation immediately. These cases compile but still require native-Wayland execution before they can be considered verified.
- X11/Xvfb remains available through the explicit `debug-x11` and `sanitizers-x11` presets for CI regression.
- C++ source/header files are each below their applicable project limit; a direct line-count scan covered tracked and untracked source files.

The available non-GUI checks are not evidence for actual framebuffer composition, physical GPU drivers, active hardware decoding, HDR, mixed-scale multi-monitor behavior, real audio output/quality, audio-video sync on hardware, or Windows packaging. `hwdec=auto-safe` requests a safe hardware-decode route; `hwdec-current` from a real playback session is the evidence of whether it was selected. See the scope and limitations in [README](../README.md).
