# Verification Record

Environment: native Linux Wayland session (niri), Qt 6.12.0, system libmpv 0.41.0 and GCC 16.2.1. The verified display is one 2880x1800 panel at 1.75 scale. Window tests use the Qt Wayland platform plugin, null audio and `MALLOC_PERTURB_=128`.

Latest debug verification (playlist integration):

- `cmake --build --preset debug` succeeded.
- `ctest --test-dir build/debug -R '^(playback|playlist)$' --output-on-failure` passed both non-GUI integration tests.
- `cmake -DNEKO_SOURCE_DIR="$PWD" -P cmake/CheckCodeSize.cmake` confirmed ordinary files are within 500 lines and the explicitly listed special files are within 800 lines.
- The playback test verifies libmpv initialization, `hwdec=auto-safe`, local-file load/seek/pause/stop, mute/volume, EOF and recovery. The playlist test verifies ordering, current-item adjustment, first valid item start, EOF advance and invalid-first-item skipping.
- GUI/window tests were deliberately not run in this verification round at the user's request. `window_test` has been updated to assert multiple command-line files start a queue, but this still requires native-Wayland execution before it can be considered verified.
- X11/Xvfb remains available through the explicit `debug-x11` and `sanitizers-x11` presets for CI regression.
- C++ source/header files are each below their applicable project limit; a direct line-count scan covered tracked and untracked source files.

The available non-GUI checks are not evidence for actual framebuffer composition, physical GPU drivers, active hardware decoding, HDR, mixed-scale multi-monitor behavior, real audio output/quality, audio-video sync on hardware, or Windows packaging. `hwdec=auto-safe` requests a safe hardware-decode route; `hwdec-current` from a real playback session is the evidence of whether it was selected. See the scope and limitations in [README](../README.md).
