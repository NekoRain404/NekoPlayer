# First Playable Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver local video playback and standard desktop controls in NekoPlayer.

**Architecture:** MpvController owns the client handle and asynchronous playback state. VideoWidget owns the OpenGL renderer and is destroyed before the controller. MainWindow binds their signals to shared actions and transport controls.

**Tech Stack:** C++20, Qt 6 Widgets/OpenGLWidgets/Test, libmpv, CMake 3.25+, FFmpeg fixtures, Xvfb.

**Spec:** `docs/superpowers/specs/2026-10-21-first-playable-design.md`, approved by user on 2026-10-21.

## Global Constraints

- C++20, Qt 6 and a single libmpv engine; installed runtime is mpv 0.41.0.
- Linux first; no new QML, plugin framework, secondary engine, playlist, HDR or Windows packaging.
- Disable user mpv configuration/scripts/OSC/default bindings. Use asynchronous commands and observed properties.
- Render callbacks only schedule work. Free the renderer with its GL context current before client shutdown.
- Keep reference repositories unchanged; write original application code.
- Preserve uncommitted bootstrap files and do not invent Git identity.

## Files and interfaces

- `src/playback/mpvcontroller.{h,cpp}`: PlaybackState, RAII client, events and commands.
- `src/playback/videowidget.{h,cpp}`: Qt OpenGL surface and renderer lifetime.
- `src/ui/mainwindow.{h,cpp}`: native menu, shared actions, transport, drag/drop and fullscreen.
- `src/main.cpp`: CLI, isolated backend diagnostic and application launch.
- `tests/playback_test.cpp`: real headless mpv integration tests.
- `tests/window_test.cpp`: actual GL frames and action/desktop integration tests.
- `tests/CMakeLists.txt`: FFmpeg-generated fixtures and Xvfb-backed test registration.
- `CMakeLists.txt`: shared playback/UI libraries, warnings and sanitizer propagation.

### Task 1: Asynchronous controller

**Interfaces:** `MpvController(bool headless=false, bool silentAudio=false, QObject *parent=nullptr)`; `isReady()`, `errorString()`, `handle()`, `state()`; commands `openFile(QString)`, `setPaused(bool)`, `togglePause()`, `stop()`, `seekAbsolute(double)`, `seekRelative(double)`, `setVolume(double)`, `setMuted(bool)`; signals `stateChanged()`, `fileLoaded()`, `playbackFinished()`, `errorOccurred(QString)`.

PlaybackState fields: `active`, `paused`, `seekable`, `muted`, `position`, `duration`, `volume`, `path`. Position/duration are seconds, volume is 0–100.

- [x] Write headless Qt tests for a generated 4-second test video. Catch missing loading, state observation, pause/seek, stop/reset, volume/mute, invalid-path recovery and EOF. Example independent expectation:
  ```cpp
  player.openFile(mediaPath);
  QTRY_VERIFY_WITH_TIMEOUT(player.state().active, 5000);
  QTRY_VERIFY_WITH_TIMEOUT(player.state().duration > 3.5, 5000);
  player.setPaused(true);
  QTRY_VERIFY(player.state().paused);
  player.seekAbsolute(2.0);
  QTRY_VERIFY(qAbs(player.state().position - 2.0) < 0.3);
  ```
- [x] Run `cmake --preset debug && cmake --build --preset debug && ctest --test-dir build/debug -R playback --output-on-failure`; establish missing-behavior failure before implementation.
- [x] Create the controller with checked initialization options; RAII `mpv_terminate_destroy`; callback lifetime remains valid until core teardown. Drain events on Qt GUI thread via queued receiver-context invocation. Track command replies, unavailable properties and file-end errors.
  ```cpp
  const char *args[]{"seek", value.constData(), "absolute+exact", nullptr};
  const int status = mpv_command_async(handle(), requestId, args);
  if (status < 0) emit errorOccurred(QString::fromUtf8(mpv_error_string(status)));
  ```
- [x] Run the focused playback tests and check original backend diagnostic remains operational.

### Task 2: Video surface and desktop interaction

**Interfaces:** `VideoWidget(MpvController &, QWidget *parent=nullptr)`; `isReady()`, signals `ready()` and `renderError(QString)`. `MainWindow(bool silentAudio=false)`; `controller()`, `videoWidget()`, `openFile(QString)`; actions named `openAction`, `pauseAction`, `stopAction`, `muteAction`, `fullscreenAction`, `exitFullscreenAction`.

- [x] Write a Qt Widgets integration test that loads an actual test pattern, waits for a nonblack GL framebuffer, pauses via QAction, seeks, resizes, toggles fullscreen and closes during playback. Verify multi-file drops do not change playback, invalid paths report a usable error, and a subsequent valid file loads.
  ```cpp
  MainWindow window(true);
  window.show();
  QTRY_VERIFY_WITH_TIMEOUT(window.videoWidget()->isReady(), 10000);
  window.openFile(mediaPath);
  QTRY_VERIFY(window.controller()->state().active);
  const auto image = window.videoWidget()->grabFramebuffer();
  QVERIFY(!image.isNull());
  ```
- [x] Run the new GL/window tests under `xvfb-run` and verify missing behavior is caught.
- [x] Implement OpenGL context creation in initializeGL, physical-pixel FBO rendering in paintGL, queued callback redraws, skip drawing when hidden, idempotent context cleanup. Use aboutToBeDestroyed and keep callback userdata alive through render-context release.
- [x] Implement a dark desktop window: native menus, central video, bottom transport controls, status text, shared actions and shortcuts. Open only after renderer is ready. Block slider feedback while seeking; disable unavailable operations. Use nonmodal error feedback so recovery works.
- [x] Add full-window mouse activity tracking for fullscreen controls; preserve normal geometry; support single local file drops and reject multiple/remote URLs clearly.
- [x] Run window/playback tests, including frame-content assertion, failure recovery, EOF, fullscreen restoration, resize and active-playback destruction.

### Task 3: Launch, verification and handoff

- [x] Replace placeholder launch with MainWindow; keep `--check-backend`; support `--smoke-test`, `--silent-audio` and one positional media path via QCommandLineParser. Show a helpful error for multiple paths.
  ```bash
  ./build/debug/NekoPlayer --check-backend
  xvfb-run -a ./build/debug/NekoPlayer --silent-audio --smoke-test
  ./build/debug/NekoPlayer '/path/电影 example.mkv'
  ```
- [x] Run full Debug and ASan/UBSan non-window regression suites; investigate sanitizer failures without blanket suppression. Window behavior tests remain skipped by user request.
- [x] Run static analysis on project source with the compiler arguments supported by clang-tidy. Review callback ownership, path encoding, errors, volume boundaries and stale property state against the spec.
- [x] Update README with implemented features, shortcuts, dependencies, test evidence and actual platform limitations. Record any unverified real-display/audio behavior precisely.
- [x] Commit only if an existing author identity is configured; otherwise leave reviewable local changes and report branch/path. Do not push or publish.

## Execution ledger

- Ruling: execute inline because client/event/render lifetimes share state and interfaces; no parallel implementation workers.
- Ruling: use an in-place feature branch because the repository has no initial commit and all bootstrap files are uncommitted; creating a worktree would first require an invented author identity or a premature baseline commit.
- Preflight: Tasks 1/2 share MpvController's declared interface; Task 2 owns the renderer and MainWindow; Task 3 only integrates CLI/build/docs. State units and constructor flags are consistent across tasks.
