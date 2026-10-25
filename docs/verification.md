# Verification Record

Playlist organization verification (2026-10-25, code at `acc623e`):

- Scope: model batch removal and stable natural title/path sorting with a shared collation helper; controller bulk removal, missing-file scan/removal and sort; pane Ctrl/Shift multi-selection, modifier-safe click-to-play and empty-state hint; and the `PlaylistMaintenanceMenu` sort submenu with count-confirmed `移除缺失文件…` cleanup. No production code changed in this documentation round.
- `cmake --preset debug` succeeded. `cmake --build --preset debug` succeeded with Ninja reporting `no work to do` on the already-built tree; the player and every test target, including the compile-only window suites, were up to date. Window and startup tests remain compile-only in the registered suite and were not executed.
- `LC_ALL=C LANG=C ctest --preset debug -E '^(window|window-startup|file-startup|.*-window)$' --output-on-failure` passed **62/62** in **60.44 s**.
- Focused selection `LC_ALL=C LANG=C ctest --preset debug -R '^(playlist|playlist-edit|playlist-pane|playlist-maintenance-menu|code-size)$' --output-on-failure` passed **5/5** in **7.27 s** (`code-size` 0.35 s, `playlist-pane` 1.78 s, `playlist-maintenance-menu` 0.12 s, `playlist` 3.89 s, `playlist-edit` 1.13 s).
- `cmake --preset sanitizers` succeeded. `cmake --build build/sanitizers --target playlist_test playlist_edit_test playlist_pane_test playlist_maintenance_menu_test` succeeded. `LC_ALL=C LANG=C ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers -R '^(playlist|playlist-edit|playlist-pane|playlist-maintenance-menu)$' --output-on-failure` passed **4/4** in **7.70 s** with no ASan/UBSan reports. Leak detection was disabled only to exclude documented platform-library exit allocations (Qt Wayland client, GTK/Pango/fontconfig); this result does not claim leak-free execution.
- `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passed **1/1** (0.35 s). Advisory output: `src/ui/windowactions.cpp` **537** lines and `tests/ui_surface_test.cpp` **550** lines, both above the 500-line target but within the 550-line ordinary hard guard; `mainwindow.cpp` remains within its listed 800-line exception.
- `cmake --build build/debug --target release_translations` succeeded with Ninja reporting `no work to do`; `/usr/lib/qt6/bin/lrelease translations/NekoPlayer_en.ts` reported **309 finished, 0 unfinished** translations.
- `clang-format --dry-run --Werror` (clang-format 23.1.3) passed for the **19** changed C++ source/header files across `e23ff29..acc623e`. `git diff --check -- ':!docs/skills.md'` passed.
- Boundary checks: no `mpv_` / `MPV_` / `mpv/client.h` usage in `src/ui/`; the phase diff adds no timers, playback polling, CPU frame copies or render-path changes; all four phase commits (`35961c2`, `cce15f8`, `8cf71e3`, `acc623e`) are authored by `NekoRain <yukathleenlxy@gmail.com>` with no co-author trailers.
- `rm -rf /tmp/neko-install && cmake --install build/debug --prefix /tmp/neko-install` installed exactly three files: `/tmp/neko-install/bin/NekoPlayer` (36 MiB Debug binary), `/tmp/neko-install/share/applications/nekoplayer.desktop`, and `/tmp/neko-install/share/icons/hicolor/scalable/apps/nekoplayer.svg`. Translations are embedded in the Qt resource, so no `.qm` install file is produced.
- Window behavior remains compile-only. This round did not run the window suite and does not claim native Wayland visual/keyboard/pointer acceptance, HDR output, GPU hardware decoding, physical-audio-device, multi-monitor mixed-scaling, Windows, or distro-packaging verification.

Settings and window verification (2026-10-25, code at `1691ca5`):

- Scope: shared `音量步进` for wheel and keyboard +/-, the persisted elapsed/remaining default, the persisted View-menu `窗口置顶` choice with mini-player restoration, `--fullscreen` / `--start-paused`, and the reviewed fix that consumes the start-paused one-shot on rejected startup opens and shows `已暂停 · %1` after the one-shot pause. `docs/skills.md` is user-owned and remained untouched.
- Final-review fix: `--fullscreen` now enters through the controlled `MainWindow::enterFullscreen()` transition after `show()` instead of a raw `showFullScreen()`, so the normal geometry is captured before fullscreen and the action/Esc/transport can exit. `tests/startup_options_window_test` now asserts entry and exit plus always-on-top-while-fullscreen restoration and was run once manually with `QT_QPA_PLATFORM=offscreen` (6/6 passed, 86 ms); this is a one-off offscreen check, not native Wayland proof and not a registered suite run.
- `cmake --preset debug` succeeded. `cmake --build --preset debug` succeeded for the player and all test targets, including the compile-only window/startup suites. Window and startup tests remain compile-only in the registered suite.
- `LC_ALL=C LANG=C ctest --preset debug -E '^(window|window-startup|file-startup|.*-window)$' --output-on-failure` passed **61/61** in **58.45 s**.
- Focused selection `LC_ALL=C LANG=C ctest --preset debug -R '^(session|interaction-preferences|preferences-dialog|preferences-coordinator|transport-bar|code-size)$' --output-on-failure` passed **6/6** in **1.49 s**.
- `cmake --preset sanitizers` (run before this final-review fix) succeeded. `cmake --build --preset sanitizers --target session_test interaction_preferences_test preferences_dialog_test preferences_coordinator_test transport_bar_test -j4` succeeded. `LC_ALL=C LANG=C ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers -R '^(session|interaction-preferences|preferences-dialog|preferences-coordinator|transport-bar)$' --output-on-failure` passed **5/5** in **2.32 s** with no ASan/UBSan reports. Leak detection was disabled only to exclude documented platform-library exit allocations (Qt Wayland client, GTK/Pango/fontconfig); this result does not claim leak-free execution.
- `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passed **1/1** (0.32 s). Advisory output: `src/ui/windowactions.cpp` **534** lines and `tests/ui_surface_test.cpp` **550** lines, both above the 500-line target but within the 550-line ordinary hard guard; `mainwindow.cpp` remains within its listed 800-line exception.
- `cmake --build --preset debug --target release_translations -j4` succeeded with Ninja reporting `no work to do`; `lrelease6 translations/NekoPlayer_en.ts` reported **296 finished, 0 unfinished** translations. The new `MainWindow` entry is `已暂停 · %1` → `Paused · %1`.
- `clang-format --dry-run --Werror` (clang-format 23.1.3) passed for the **18** changed C++ source/header files across `a4550a6..1691ca5`. `git diff --check a4550a6..1691ca5 -- . ':(exclude)docs/skills.md'` passed.
- Boundary checks: no `mpv_` / `MPV_` / `mpv/client.h` usage in `src/ui/`; no new UI timers or playback polling, CPU frame copies, or raw mpv commands; every phase commit is authored by `NekoRain` with no co-author trailers.
- `rm -rf /tmp/neko-install && cmake --install build/debug --prefix /tmp/neko-install` installed exactly three files: `/tmp/neko-install/bin/NekoPlayer`, `/tmp/neko-install/share/applications/nekoplayer.desktop`, and `/tmp/neko-install/share/icons/hicolor/scalable/apps/nekoplayer.svg`.
- Window behavior in the registered suite remains compile-only. The one-off offscreen startup-window run recorded above is not native Wayland visual/keyboard/pointer acceptance, HDR output, GPU hardware decoding, physical-audio-device, multi-monitor mixed-scaling, Windows, or distro-packaging verification.

Final post-review playlist/discovery regression (2026-10-25, code at `1cf0480`):

- The final review fix wave exposed the pane's Ctrl+F/Esc search actions through the F1 reference, disarmed stop-after-current when a queue finishes through failure/exhaustion, added remove-current and mode-menu regressions, cleared stale search text on queue replacement, and disabled the window-level exit-fullscreen Escape action outside fullscreen to remove shortcut ambiguity.
- `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **58/58** in **58.07 s** on the final tree. The focused post-fix selection passed **7/7**; focused ASan/UBSan passed **7/7** with `ASAN_OPTIONS=detect_leaks=0` (documented platform-library caveat); after the final Escape fix, the affected `playlist-pane`, `shortcuts-dialog`, and `playlist-mode-menu` selection passed **3/3**. Window tests were compiled only.
- `lrelease` reports **293** translations, 0 unfinished. Native Wayland visual/keyboard/pointer acceptance remains open; a launch-context probe showed even a minimal Qt widget from the agent shell is not mapped by the running niri session (the same binary maps under XWayland), so visual acceptance requires an interactive desktop session.

Playlist and discovery verification (2026-10-25, code at `3a5ab8c`):

- Scope: playlist search/filter, the live F1 shortcut reference and Help menu, stop-after-current, and the runtime application icon. No production code changed in this documentation round.
- `cmake --preset debug` succeeded. `cmake --build --preset debug -j4` succeeded and relinked all 23 build targets, including the player and every test executable; the window-behavior suites were compiled and linked but not executed.
- Standard suite `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **57/57** in **66.37 s**. The selection excluded 14 window/desktop tests out of 71 registered tests. The three new targets are `playlist-pane`, `shortcuts-dialog`, and `app-icon`; stop-after-current cases live in `playlist-modes`. `LC_ALL=C` remains required because the host's non-C locale makes mpv reject playback.
- Focused selection `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(code-size|playlist|playlist-edit|playlist-modes|playlist-pane|shortcuts-dialog|app-icon|preferences-dialog)$' --output-on-failure` passed **8/8** in **11.44 s**.
- `cmake --preset sanitizers` succeeded. `cmake --build --preset sanitizers --target playlist_test playlist_edit_test playlist_modes_test playlist_pane_test shortcuts_dialog_test app_icon_test -j4` succeeded. `LC_ALL=C LANG=C ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers -R '^(playlist|playlist-edit|playlist-modes|playlist-pane|shortcuts-dialog|app-icon)$' --output-on-failure` passed **6/6** in **11.73 s** with no ASan/UBSan reports. Leak detection was disabled only to exclude documented platform-library exit allocations (Qt Wayland client, GTK/Pango/fontconfig); this result does not claim leak-free execution.
- `rm -rf /tmp/neko-install && cmake --install build/debug --prefix /tmp/neko-install` installed exactly three files: `/tmp/neko-install/bin/NekoPlayer`, `/tmp/neko-install/share/applications/nekoplayer.desktop`, and `/tmp/neko-install/share/icons/hicolor/scalable/apps/nekoplayer.svg`.
- `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passed **1/1** (0.40 s). The guard enforces a 500-line target, a 550-line ordinary hard limit, and 800 lines for the listed `mainwindow.cpp` / `window_test.cpp` exceptions.
- `clang-format --dry-run --Werror` (clang-format 23.1.3) passed for the **18** changed C++ source/header files that still exist across `7e098ad..3a5ab8c`. `git diff --check -- . ':(exclude)docs/skills.md'` passed for both the working tree and the `7e098ad..3a5ab8c` range.
- `cmake --build --preset debug --target release_translations -j4` succeeded with Ninja reporting `no work to do`; `lrelease6 translations/NekoPlayer_en.ts` reported **291 finished, 0 unfinished** translations.
- Boundary checks: no `mpv_` / `MPV_` / `mpv/client.h` usage in `src/ui/`; UI timers remain single-shot hide/feedback or click-disambiguation timers rather than playback polling; no CPU frame copies or render-path changes.
- Not verified by this round: native Wayland visual/keyboard/pointer behavior, HDR output and tone mapping on an HDR display, GPU hardware decoding, physical audio devices, multi-monitor mixed scaling, Windows, and distro packaging. Window-behavior tests compiled only. Real-display acceptance remains open.

Final post-review hardening regression (2026-10-25, code at `6232353`):

- The final fix wave scoped PreferencesDialog Restore Defaults by category (Subtitles no longer resets the hidden Playback seek draft), added a load-bearing regression for that path, asserted playlist display-name cache alignment, normalized the programmatic seek setter, and added a custom-seek save/load assertion.
- `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **54/54** in **58.91 s** on the final tree. The focused post-fix selection passed **5/5**; window tests were compiled only.
- The whole-branch reviewer found no Critical issues, one Important (cross-page Restore Defaults) and six Minor findings; all Important and cheap Minor findings were fixed, with the remaining deferred items recorded below. A scoped re-review confirmed every fix.

Release-hardening final verification (2026-10-25, code at `1d582ed`):

- Scope: removal of the production-dead `PlaybackSettingsDialog` / `SubtitleSettingsDialog`, the narrow-width transport time fix, the playlist display-name cache, the custom 1–120 s seek interval, and GNUInstallDirs install/desktop/icon integration. No production code changed in this documentation round.
- `cmake --preset debug` succeeded. `cmake --build --preset debug -j4` succeeded with Ninja reporting `no work to do` on the already-built tree; the player and every test target, including the compile-only window suites, were up to date. Window-behavior tests were compiled but not executed.
- `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **54/54** in **58.62 s**. The selection excluded 14 window/desktop tests (`preferences-dialog`, `video-color-dialog`, `file-startup`, and eleven `*window*` suites) out of 68 registered tests. `LC_ALL=C` remains required because the host's non-C locale makes mpv reject playback.
- Focused selection `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(code-size|preferences-dialog|preferences-coordinator|transport-bar|playlist|playlist-edit|session)$' --output-on-failure` passed **7/7** in **5.94 s**.
- `cmake --preset sanitizers` and `cmake --build --preset sanitizers --target preferences_dialog_test transport_bar_test playlist_test playlist_edit_test session_test -j4` succeeded. `LC_ALL=C LANG=C ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers -R '^(preferences-dialog|transport-bar|playlist|playlist-edit|session)$' --output-on-failure` passed **5/5** in **6.65 s** with no ASan/UBSan reports. Leak detection was disabled only to exclude documented platform-library exit allocations (Qt Wayland client, GTK/Pango/fontconfig); this result does not claim leak-free execution.
- `rm -rf /tmp/neko-install && cmake --install build/debug --prefix /tmp/neko-install` installed exactly three files: `/tmp/neko-install/bin/NekoPlayer`, `/tmp/neko-install/share/applications/nekoplayer.desktop`, and `/tmp/neko-install/share/icons/hicolor/scalable/apps/nekoplayer.svg`.
- `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passed **1/1** (0.32 s). The guard enforces a 500-line target, a 550-line ordinary hard limit, and 800 lines for the listed `mainwindow.cpp` / `window_test.cpp` exceptions.
- `clang-format --dry-run --Werror` (clang-format 23.1.3) passed for the **16** changed C++ source/header files that still exist across `7357e35..1d582ed`. `git diff --check -- . ':(exclude)docs/skills.md'` passed.
- `cmake --build --preset debug --target release_translations -j4` succeeded with Ninja reporting `no work to do`; `lrelease6 translations/NekoPlayer_en.ts` reported **280 finished, 0 unfinished** translations.
- Boundary checks: no `mpv_` / `MPV_` / `mpv/client.h` usage in `src/ui/`; the remaining UI timers are single-shot hide, feedback, or click-disambiguation timers rather than playback polling; no CPU frame copies or render-path changes.
- Not verified by this round: native Wayland visual/keyboard/pointer behavior, HDR output and tone mapping on an HDR display, GPU hardware decoding, physical audio devices, multi-monitor mixed scaling, Windows, and distro packaging. Window-behavior tests were not run.

Final post-review regression (2026-10-24, code at `987d46a`):

- The final test-only fixes changed no production code. The standard non-window selection `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **54/54** in **58.50 s** on the final tree. Window-behavior tests were compiled but not executed.
- The final review fix wave resolved the `--no-config` Left/Right seek-step fallback, the playlist single-click precise timer, the missing Interaction Settings icon, the Interaction help text and translation, and the flaky track-restart test (10/10 focused runs). Remaining limitations are recorded in the first section below.

Release-readiness known limitations after final review (2026-10-24):

- Narrow-width transport label (superseded by release hardening): the earlier 480–639 px band capped the time label at 100 px while the displayed text needed roughly 137–144 px, so the remaining-time mode could clip. The hardened layout now hides the time label below 560 px and allocates 155 px at 560–639 px; metrics and offscreen widget tests cover 480, 559, 560, 639, and 640 px. Native Wayland visual recheck remains outstanding.
- Test-file size headroom: `tests/ui_surface_test.cpp` is exactly 550 lines and `tests/session_test.cpp` is 548 lines against the 550-line ordinary-file hard limit. The next addition to either suite must split it by responsibility or revisit the guard; do not compress formatting to fit.

Release-readiness final verification (2026-10-24, code at `ebfbb21`):

- Scope: six-page unified preferences dialog, configurable click/wheel interaction, clickable transport time, resizable playlist with persisted width, and the GUI-thread performance caches. No production code changed in this round except the missing English entry for `播放时间`.
- `cmake --preset debug` succeeded. `cmake --build --preset debug --clean-first -j4` rebuilt every target, including `window_test`; the translation resources were regenerated. Window-behavior tests were compiled but not executed.
- `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **54/54** in **57.50 s** on the clean rebuild. `LC_ALL=C` is required because the host's non-C locale makes mpv reject playback.
- Focused selection `LC_ALL=C LANG=C ctest --test-dir build/debug -R '^(code-size|preferences-dialog|preferences-coordinator|appearance-preferences|ui-surface|transport-bar|interaction-policy|session|folder-menu|title-format|playlist-edit)$' --output-on-failure` passed **11/11** in **6.56 s**.
- `cmake --preset sanitizers` and `cmake --build --preset sanitizers --target preferences_dialog_test preferences_coordinator_test interaction_policy_test transport_bar_test session_test folder_menu_test -j4` succeeded. `LC_ALL=C LANG=C ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers -R '^(preferences-dialog|preferences-coordinator|interaction-policy|transport-bar|session|folder-menu)$' --output-on-failure` passed **6/6** in **2.46 s** with no ASan/UBSan reports. Leak detection was disabled only to exclude documented platform-library exit allocations (Qt Wayland client, GTK/Pango/fontconfig); this result does not claim leak-free execution.
- `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` passed **1/1** (0.33 s). The guard enforces a 500-line target, 550-line ordinary hard limit, and 800 lines for the listed `mainwindow.cpp` / `window_test.cpp` exceptions.
- `clang-format --dry-run --Werror` (clang-format 23.1.3) passed for all **40** C++ source/header files changed by `332f08d..ebfbb21`. `git diff --check -- . ':(exclude)docs/skills.md'` passed.
- Translation catalog `translations/NekoPlayer_en.ts` now contains the `TransportBar` entry `播放时间` → `Playback time`. `lrelease` reported **277 finished, 0 unfinished** translations, and the build regenerated `NekoPlayer_en.qm` (28,744 bytes).
- Boundary checks: no `mpv_`/`MPV_`/`mpv/client.h` usage in `src/ui/`; the new click timers are single-shot disambiguation, not polling; no CPU frame copies or render-path changes.
- Not verified by this round: native Wayland visual/keyboard/pointer behavior, real HDR output and tone mapping on a HDR display, GPU hardware decoding, physical audio devices, multi-monitor mixed scaling, and Windows packaging. Window-behavior tests were not run.
- This section supersedes the older three-page preferences descriptions in earlier records; historical entries below are left unchanged.

Welcome-panel vertical-overflow correction and width-boundary regression (2026-10-24):

- A new offscreen geometry regression reproduced the drop-hint intersection with the primary buttons at a 400×215 logical-pixel card height. The cause was showing supporting copy at a height where the visible labels and buttons could not fit.
- Supporting copy now remains hidden until 260 px. The regression checks 24 size combinations across widths 236–500 px and heights 215–380 px, with the maximum three recent-media and three resume rows present; any visible hint must begin below both primary actions.
- `ui-surface` and `code-size` passed; changed-file Clang formatting and `git diff --check` passed. No window-behavior tests were run. This confirms widget geometry under Qt offscreen, not the final native Wayland composition; visual desktop recheck remains outstanding.

完整 Debug 非窗口回归复核（2026-10-24）：

- `cmake --build --preset debug -j4` 完成，播放器与全部测试目标均构建成功。
- `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` 通过 **50/50**（54.13 秒），包含 code-size、UI 主题/图标、媒体扫描、网络播放、缩略图、逐帧、播放列表、续播、字幕、音轨、视频呈现与色彩控制等测试。
- 按用户要求未运行窗口行为测试。此次结果不验证原生 Wayland 窗口交互、实际 HDR 输出、GPU 硬解或真实音频设备表现；这些仍需桌面环境专项验收。

完整 ASan/UBSan 非窗口回归复核（2026-10-24）：

- `cmake --build --preset sanitizers -j4` 完成，播放器及全部测试目标均构建成功。
- `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/sanitizers --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` 通过 **50/50**（65.29 秒），未报告 ASan/UBSan 错误。关闭进程退出泄漏检查是为了排除先前记录的平台库缓存分配；本结果不表示泄漏检测通过。
- 按用户要求未运行窗口行为测试。此次结果不验证原生 Wayland 窗口交互、实际 HDR 输出、GPU 硬解或真实音频设备表现；这些仍需桌面环境专项验收。

First playable verification and static-analysis closure (2026-10-24):

- Fixed a Clang-only most-vexing-parse issue in `mediafolderscan.cpp` by using brace initialization for `QCollator`; the independent Clang build of `neko_input` and that file's Clang-Tidy check passed.
- Full Debug build and all **49/49 non-window tests** passed. Full ASan/UBSan build and the same **49/49 non-window tests** passed with `ASAN_OPTIONS=detect_leaks=0`. A run without that setting reported process-exit allocations rooted in host GTK/Pango/fontconfig platform libraries, so it was stopped and not counted as a project leak result.
- Clang-Tidy processed all **80 production `.cpp` files** with `clang-analyzer-*` and `bugprone-narrowing-conversions`. No optional-access or unsafe-path findings were emitted. The folder-scan watcher leak warning is a Qt-parent-ownership false positive: `QFutureWatcher` is constructed with the job as QObject parent and also `deleteLater()`'d when its future finishes. Reviewed narrowing reports cover Qt model APIs whose row counts are `int`, timestamp-to-seconds and byte-size display conversions, and Qt key-sequence construction; these are bounded/interface or intentional precision conversions, not blindly rewritten. The analyzer also reports private `MpvController::Impl` padding; this per-player aggregate is dominated by Qt containers/strings and was not reordered without evidence of a meaningful performance gain.
- `code-size`, `clang-format --dry-run --Werror` for the changed source, and `git diff --check` passed. No GUI/window tests were run, per user request.

完整非窗口回归阶段（2026-10-24）：

- 在容器-size 收尾提交后重新构建完整 Debug 目标；`ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` 通过 **49/49**（63.50 秒）。
- 完整 ASan/UBSan 目标构建成功；相同的非窗口 CTest 选择在 `ASAN_OPTIONS=detect_leaks=0` 下通过 **49/49**（80.87 秒）。保持 ASan/UBSan 的地址与未定义行为检测；关闭进程退出泄漏检查是为了避开 Qt Wayland 客户端、GTK/Pango/fontconfig 等平台库保留的外部缓存/代理分配。未据此宣称泄漏检查通过。
- 单独复跑首轮出现外部退出泄漏报告的 UI 与缩略图测试，四项通过 **4/4**。其中 `ui-surface` 的窄宽布局用例在 sanitizer 的原始宿主 GTK 环境偶发未满足方向断言，在规定的 generic Qt 主题下通过。
- 按用户要求未运行窗口行为测试；Wayland 实机布局、输入路由、GPU、HDR 输出与硬件解码仍未由本轮验收。测试选择包含 `code-size`、主题/图标、网络、缩略图、截图、逐帧、区间循环、播放列表、续播、轨道/字幕、文件拖放路由及视频色彩控制器等 49 个非窗口测试。

Container-size conversion cleanup stage (2026-10-24):

- `ThumbnailService::cacheSize()` now exposes Qt's `qsizetype` rather than narrowing a container size to `int`; a compile-time API regression check guards that contract. Session path normalization also tracks Qt list positions as `qsizetype` and converts only at the existing `int` index boundary.
- Full Debug build succeeded; `thumbnail-service`, `session`, and `code-size` passed **3/3**. The corresponding ASan/UBSan tests passed **2/2**. Clang's `bugprone-narrowing-conversions` check reported no findings in the two affected translation units; changed-file formatting and `git diff --check` passed.
- Window behavior tests were not run. This is a type-safety cleanup only and does not validate native Wayland, graphics, or hardware playback behavior.

HDR 元数据测试与硬解隔离阶段（2026-10-24）：

- HDR 元数据用例在打开合成样本前显式关闭硬件解码，避免测试结果受 GPU／驱动初始化耗时影响；生产播放默认值及 HDR 渲染路径不变。
- 定向媒体信息测试在 Debug 下通过（2.27 秒），在 ASan/UBSan 下通过（2.63 秒）。完整 Debug 非窗口套件通过 **50/50**；完整 ASan/UBSan 非窗口套件通过 **50/50**（61.01 秒，`ASAN_OPTIONS=detect_leaks=0`）。
- `code-size`、`clang-format --dry-run --Werror` 和 `git diff --check` 通过。按用户要求未运行窗口行为测试；这不构成真实 Wayland、GPU 或 HDR 输出验收。

媒体文件筛选器一致性阶段（2026-10-24）：

- 将视频与音频支持扩展名集中到 `mediafiletypes.cpp`；目录扫描以及打开/添加文件对话框复用同一集合，防止可扫描文件被选择器过滤掉。回归测试逐项核对 30 种扩展名及对话框通配符。
- Debug 全量构建通过；`ctest --test-dir build/debug -E 'window|startup' --output-on-failure` 通过 **50/50**。`code-size` 在该测试集中通过；`clang-format --dry-run --Werror` 与 `git diff --check` 通过。
- 按用户要求未运行窗口行为测试。验证覆盖过滤器和目录扫描契约，不代表真实 Wayland 文件选择器视觉验收；播放队列语义及 libmpv/GPU 路径未改变。

Thumbnail timestamp origin correction stage (2026-10-24):

- FFmpeg thumbnail seek targets and returned timestamps now use media-relative time even when a
  stream's container timestamps begin at a non-zero offset. A generated 10-second-offset MKV
  regression failed before the correction (`0.8 s` preview returned a timestamp above `1.0 s`)
  and passed afterward. Overflow while adding the stream origin is rejected explicitly.
- The complete Debug build succeeded. Focused `thumbnail-decoder`, `thumbnail-service`,
  `thumbnail-preview`, and `frame-step` tests passed **4/4**; ASan/UBSan decoder, service and
  frame-step tests passed **3/3**. The full non-window Debug selection passed **49/49** in
  **53.55 seconds**. `code-size`, Clang formatting and `git diff --check` passed.
- Window behavior tests were not run, per the standing preference. The generated fixture uses
  FFmpeg and is independent of Wayland, libmpv rendering and hardware decoding.

Thumbnail preview eligibility stage (2026-10-24):

- Timeline hover now requests thumbnails only for active, seekable local media with a finite
  positive duration and an available motion-video stream. Seekable audio no longer starts a
  decoder task on every hover; network preview remains intentionally unsupported.
- The new policy regression covers audio-only media, valid local video, network sources, and
  inactive/non-seekable/zero-duration playback. The full Debug build and non-window suite passed
  **50/50** in **53.23 seconds**; ASan/UBSan policy test passed **1/1**. `code-size`, Clang
  formatting and `git diff --check` passed.
- Window behavior tests were not run. The contract test exercises the eligibility decision only;
  actual Wayland popup positioning and FFmpeg cancellation latency remain desktop/runtime checks.

首页续看历史阶段（2026-10-24）：

- 欢迎页新增“继续观看”区块，消费 `SessionManager` 已加载／更新的本地续播记录；最多展示 3 项，排除非法路径、无效时长、越界位置以及距片尾不超过 1 秒的条目。按钮沿用已有 `openFile` 和播放队列续播解析，不建立第二份存储，也不触碰网络媒体。
- 完整 Debug 构建通过；`ui-surface`、`appearance-preferences`、`playback-history-store`、`playback-history-session`、`session`、`code-size` 共 **6/6** 通过。修改文件的 clang-format 与 `git diff --check` 通过，普通代码文件均低于 500 行。
- 未运行窗口行为测试；首页在真实 Wayland 下的布局/视觉及跨会话桌面验收仍未覆盖。本阶段不改变 mpv 命令、解码器、HDR 或视频渲染路径。

Media-first UI visual foundation stage (2026-10-24):

- Added semantic Dark/Light tokens, token-generated main-window styling, WCAG AA primary-action and muted-text contrast, and shared DPR-aware vector icons. The icon review regression verifies outline interiors remain transparent and seek directions differ. Dark/Light primary-action contrast is 6.84:1 / 5.04:1; Light muted text is 4.64:1 on the canvas and 5.02:1 on the surface.
- `cmake --build --preset debug -j4` succeeded. `ctest --test-dir build/debug -R '^(code-size|appearance-preferences|ui-surface)$' --output-on-failure` passed **3/3**. `git diff --check` and changed-line `git clang-format` checks passed. Ordinary handwritten files remain within the 500-line limit; the existing `mainwindow.cpp` exception is 581/800 lines.
- A later static capture on native Wayland (`eDP-1`, 2880×1800) using `wf-recorder` showed the welcome-panel drop/shortcut hint visually colliding with the bottom edge of the two primary entry buttons. This fails visual acceptance; widget-geometry intersection has not yet been measured, so the underlying layout cause remains to be reproduced and confirmed before fixing. The capture was inspected from a `--no-config` launch; no menu/playback interaction was performed and no window-behavior tests were run. No screenshot is committed because the desktop capture contains unrelated host UI.
- A second static capture used the generated `猫 sample [1].mkv` fixture from the command line. It showed a real video frame and the transport bar on Wayland, but the frame timestamp was 3.960 seconds at the four-second fixture's end; this verifies a rendered frame was visible, not a stable in-progress playback state. A folder-scan progress dialog was also visible, but no initiating action could be established from the capture; code inspection shows scan starts are connected to explicit folder actions/drop handling, so no causal attribution is recorded.
- At the time of this capture, the welcome-surface collision had not yet been resolved. The 2026-10-24 geometry-correction entry above records the later fix and its offscreen regression coverage; native Wayland visual recheck remains outstanding. No stable in-progress playback, light-theme, narrow-window, or alternate-scale capture has been accepted. `wf-recorder` is present in the current environment.

Transport-bar visual hierarchy stage (2026-10-24):

- The play/pause control is now a distinct primary action with a larger icon and theme-aware
  resting, hover, pressed and checked states. Secondary controls retain their compact sizing;
  row spacing is slightly more generous without changing responsive hide/show thresholds or
  playback wiring.
- `cmake --build build/debug -j4` succeeded. `ctest --test-dir build/debug -R
  '^(code-size|appearance-preferences|ui-surface|fullscreen-feedback-window)$'` passed **4/4**.
- The primary-action contract verifies the button still invokes the pause QAction and is visually
  distinguished by its icon size. Appearance checks exercise both dark and light primary states;
  the fullscreen feedback test covers the existing overlay/control lifecycle.
- Formatting, `git diff --check` and the 500/800-line checker passed. Real Wayland visual
  inspection of color/contrast and hover/focus presentation remains outstanding.

Reply-queue exhaustion test stabilization (2026-10-24):

- Repeated testing exposed that a single event-pump/probe sequence did not guarantee libmpv's
  bounded reply queue had room for all three retry commands. The regression test now retries each
  identical framing request until that specific request is accepted, then verifies native values.
- `video-framing` passed five consecutive focused runs. The full non-window Debug selection
  (`ctest --test-dir build/debug -E 'window|startup' --output-on-failure`) passed **49/49**.
- Formatting and `git diff --check` passed. Production behavior was unchanged.

Wayland file-drop window-test readiness stage (2026-10-23):

- Removed a redundant `qWaitForWindowExposed()` assertion from the desktop drag/drop integration test. Under the configured native Wayland backend it timed out even though the scenario's actual prerequisite is successful `VideoWidget` renderer initialization; the test now waits for that concrete condition before exercising file-drop routing.
- `file-drop-window` passed ten consecutive repetitions. The full Debug suite subsequently passed **62/62** in **87.89 seconds**; the transient `track-preference-session` and `video-framing` failures from the preceding run did not recur. The changed window checks and `code-size` also passed in focused runs. `clang-format --dry-run --Werror` and `git diff --check` passed.

Playlist navigation window-test readiness stage (2026-10-23):

- The `playlist-window` shuffle/navigation case previously expected navigation actions to be enabled before its `QOpenGLWidget` renderer existed. The test now verifies actions remain disabled before renderer readiness, then waits for the visible renderer and checks the queue-dependent action state. This matches the documented playback-entry readiness boundary; production behavior was unchanged.
- `playlist-window` passed five consecutive repetitions after the fixture correction. It and the general `window` suite passed in focused runs; see the full 62/62 result above.

Fullscreen presentation and preview lifecycle stage (2026-10-23):

- `cmake --build --preset debug --target fullscreen_feedback_window_test window_test -j2` succeeded. `ctest --test-dir build/debug -R '^(fullscreen-feedback-window|window|code-size)$' --output-on-failure` passed 3/3. `clang-format --dry-run --Werror` and `git diff --check` passed.
- The full `ctest --preset debug --output-on-failure` run passed 60/62. Two unrelated desktop-test failures remain: `file-drop-window` could not expose its window in this environment (`qWaitForWindowExposed`), and `playlist-window` failed its shuffle/navigation assertion. The changed fullscreen and general window suites both passed in the same run.
- New regressions cover direct external fullscreen/normal state synchronization, playlist/menu/status restoration, preview dismissal on fullscreen/mini/seek-press/playback-finish, and clean transitions between mini-player and fullscreen in both directions. The fullscreen transport assertion reflects the tested behavior: controls are visible on entry and subsequently fade.
- This verifies Qt window state in the configured desktop test runner, not popup positioning or animation under a real Wayland compositor. Hardware video rendering and HDR are outside this UI lifecycle stage.

Timeline thumbnail preview stage (2026-10-23):

- The transport timeline now reports hover time plus global pointer position. `MainWindow` uses
  that signal to request a local thumbnail and presents it in a non-activating floating preview;
  the popup is clamped to the active screen and falls below the timeline when there is no room
  above it. Fullscreen reparenting remains geometry-safe because the anchor is mapped globally.
- Preview painting stays on the GUI thread and consumes `QImage` from the isolated service. Source
  replacement, stop and timeline leave cancel pending work, while stale results are rejected by
  the service generation and current media path. Network thumbnails remain intentionally disabled.
- `appearance-preferences`, `thumbnail-preview`, `thumbnail-service`, `thumbnail-decoder` and
  `code-size` passed. Formatting and `git diff --check` passed; no libmpv, HDR, OpenGL or Wayland
  video-frame path changed.
- The complete non-window Debug collection passed **48/49** in **55.10 seconds**; the existing
  `video-framing` test had one timing-sensitive failure during the parallel run and passed on the
  immediate focused rerun. No new failure was observed in the thumbnail or UI tests.

Thumbnail request service stage (2026-10-23):

- Added a single-worker `ThumbnailService` with cancellation, generation checks, local-file policy,
  bounded in-memory cache and GUI-thread result delivery. Network URLs are rejected deliberately
  until preview access can share mpv authentication and buffering policy.
- `thumbnail-service`, `thumbnail-decoder` and `code-size` passed. The service regression covers
  network rejection, latest-request wins, cancellation suppression and cache reuse. Formatting and
  `git diff --check` passed; the primary libmpv hardware decode, HDR and Wayland render paths are
  untouched.

Transport grouping polish stage (2026-10-23):

- The bottom transport now separates playback controls, time readout and audio/window controls
  with stable, theme-aware dividers. The time divider follows the responsive visibility policy,
  so narrow windows do not leave an orphaned line or shift the control row.
- `appearance-preferences` passed with the new dark/light style contracts. Code-size, Clang
  formatting and `git diff --check` passed. No playback commands, layout geometry of the video
  surface, decoder settings or GPU rendering path changed.

Thumbnail decoder foundation stage (2026-10-23):

- Added an isolated FFmpeg thumbnail decoder for local media. It owns one format/codec/scaler
  context per request, supports cancellation through FFmpeg's interrupt callback, and returns
  GUI-safe `QImage` data without touching libmpv, OpenGL, Wayland surfaces, HDR presentation or
  the primary hardware-decoding path.
- The focused `thumbnail-decoder` CTest passed; the generated 320x180 fixture verifies bounded
  output dimensions and invalid/cancelled request handling. Clang formatting, `git diff --check`
  and the 500/800-line code-size check passed. Network thumbnail access remains intentionally
  deferred until a request policy can preserve mpv authentication and avoid blocking previews.

Playlist narrow text stage (2026-10-23):

- Playlist rows now use middle elision so long entries retain both the meaningful filename prefix
  and the extension at the 240 px pane minimum. The model's full normalized path remains available
  through `Qt::ToolTipRole`; queue order, drag/drop and playback behavior are unchanged.
- The new narrow-pane regression failed against Qt's default `ElideRight` and passes with the
  explicit policy. `ui-surface` and the full non-window Debug selection (`ctest --test-dir
  build/debug -E 'window|startup'`) passed **46/46** in **54.66 seconds**; code-size, formatting
  and `git diff --check` passed. Native Wayland text rasterization remains desktop verification.

Runtime language refresh stage (2026-10-23):

- Settings now retranslate as one subtree when the language changes at runtime: theme menu items,
  playback settings, subtitle display settings, and About keep their existing actions, shortcuts,
  enabled state and theme check state. Subtitle display menu actions now expose a dedicated
  `retranslate()` path and include English catalog entries.
- Preferences categories now show Qt standard playback, audio and subtitle icons for faster visual
  scanning without changing the three-page data flow.
- The new window regression passes English -> Chinese round-tripping and preserves the playback
  settings shortcut and selected theme. `preferences-dialog` and `window` passed **2/2** in the
  focused run; the full non-window Debug selection (`ctest --test-dir build/debug -E 'window|startup'`)
  passed **46/46** in **58.81 seconds**. Code-size, Clang formatting and `git diff --check` passed.
- No playback commands, decoder settings, GPU render path, CPU frame copies or HDR handling changed.
  The window test was run under the native Wayland session; broader window-suite failures recorded
  in the previous stages remain outside the non-window regression scope.

Transport hierarchy stage (2026-10-23):

- The transport bar now has a restrained top separator in both themes, a distinct monospace time
  readout, and an independently styled volume slider. Timeline sizing, direct seek behavior,
  keyboard shortcuts and shared QAction wiring are unchanged.
- The appearance regression now asserts the transport, time and volume style contracts. The full
  non-window Debug selection (`ctest --test-dir build/debug -E 'window|startup'`) passed **46/46**
  in **55.29 seconds**; code-size, Clang formatting and `git diff --check` passed.
- No libmpv commands, decoder settings, GPU render path, CPU frame copies, HDR handling or
  playback polling changed. Native Wayland pixels and pointer hover rendering remain unverified.

Welcome panel hierarchy stage (2026-10-23):

- The empty-state surface now has a small NekoPlayer brand mark, more deliberate vertical rhythm,
  a slightly lighter container radius, and taller recent-history rows. The brand mark is hidden only
  for extremely short surfaces, preserving the primary open actions before supporting copy.
- `ui-surface` and `appearance-preferences` passed **2/2** in the focused Debug run. The full
  non-window Debug selection (`ctest --test-dir build/debug -E 'window|startup'`) passed **46/46**
  in **54.98 seconds**; code-size, formatting and `git diff --check` passed. No libmpv commands,
  decoder settings, GPU render path, CPU frame copies, HDR handling or playback polling changed.
- A full unfiltered Debug CTest run was also attempted under the native Wayland session. Four
  pre-existing window targets failed (fullscreen feedback, media-information teardown, file-drop,
  and playlist-window); these are outside the standing non-window verification scope and are not
  attributed to this welcome-panel-only change. Native Wayland visual spacing remains unverified.

Mini-player session geometry follow-up (2026-10-23):

- Closing the app while mini mode was active previously persisted the temporary compact geometry as the normal window size. Mini mode now captures the pre-entry Qt geometry blob and session saving persists that value, while the existing `QRect` still restores the visible window when leaving mini mode.
- The new test failed before the fix by comparing the stored session geometry with the normal geometry captured before mini mode; both the persistence regression and mini-mode toggle test pass after the fix. Full Debug build, code-size, formatting and `git diff --check` passed.
- Tests ran offscreen and report unsupported QOpenGLWidget/raise operations; they validate saved Qt state, not actual Wayland placement or video rendering.
- After the follow-up, the full non-window Debug CTest selection (`-E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'`) passed **45/45** in **53.33 seconds**. Window behavior tests remain outside this suite per the standing preference.

Mini-player window mode stage (2026-10-23):

- Added a shared `Ctrl+Shift+M` action in View and the video context menu. It reuses the existing main window, libmpv instance and GPU-rendered video widget; mini mode hides menus/status/playlist, uses a 480×320 tool window with a stay-on-top hint, and restores the saved normal geometry, maximized state and playlist visibility on exit.
- Entering from fullscreen exits fullscreen first. The action remains checkable, changes to “Exit Mini Player” while active, and is translated at runtime. No extra decoder, frame copy, or polling path is introduced.
- The new Qt window-state regression passed under the offscreen plugin, including action state, hidden chrome, flags and size restoration. This environment reports that `QOpenGLWidget` and native raise are unsupported, so it does not verify video pixels, actual stacking, or compositor policy. Wayland may ignore or constrain always-on-top requests; that remains native desktop verification.
- Full Debug build, focused `ui-surface`, `appearance-preferences`, `code-size`, formatting and `git diff --check` passed. No native Wayland window test was run.

Welcome panel icon and interaction stage (2026-10-23):

- Recent local and network entries now use distinct Qt standard icons, a consistent 16 px icon size, a pointing-hand cursor, and keyboard-friendly focus behavior without changing the existing click signal.
- Accessible names identify the media type and retain the sanitized location; runtime language changes update these names along with the visible heading. Tooltips remain free of network credentials, queries and fragments.
- The red/green UI regression covers icon presence, size, sanitized network metadata and English runtime retranslation. `ui-surface_test` passed **8/8** in Debug; formatting and `git diff --check` passed. Window behavior tests were not run, so native Wayland icon rasterization and pointer/focus visuals remain desktop verification work.
- No libmpv commands, decoder settings, GPU render path, CPU frame copies, HDR handling or playback polling changed in this stage.

Welcome panel recent-history stage (2026-10-23):

- The welcome surface now shows up to three recent local or network sources, keeps network credentials, query strings and fragments out of labels/tooltips, and emits the original source when a history action is selected.
- Recent history updates live from `RecentMediaHistory`, and the empty state remains clear when no entries exist. Runtime language changes translate the recent-history heading and fallback label.
- The card now yields recent entries and supporting copy at short heights while preserving both primary playback actions; wide cards keep the compact two-column action layout and narrow cards stack actions vertically.
- `ui-surface_test` passed **8/8** in Debug and ASan/UBSan with offscreen Qt (`ASAN_OPTIONS=detect_leaks=0`). The short-height regression covers the 360×190 surface and the existing narrow-width regression covers the stacked actions. `appearance-preferences`, `code-size`, formatting and `git diff --check` also passed. Window behavior tests were not run; native Wayland visual spacing and history-click behavior remain desktop verification work.
- No libmpv commands, decoder settings, GPU render path, CPU frame copies, HDR handling or playback polling changed in this stage.

Welcome panel narrow-layout stage (2026-10-23):

- The welcome card now switches its primary actions from a row to a column when the translated button labels no longer fit side-by-side. Its minimum width was reduced so the stacked layout fits the supported narrow video surface with the playlist open.
- `ui-surface` passed in Debug and ASan/UBSan (`ASAN_OPTIONS=detect_leaks=0`); the new offscreen regression checks the 280 px card width where the network action previously had only 118 px for a 168 px size hint. `code-size`, `appearance-preferences`, and `preferences-dialog` passed **4/4**; the full Debug build, formatting and `git diff --check` passed. Window behavior tests were compiled but not run; native Wayland layout remains desktop verification work.
- Running the sanitizer test with default LeakSanitizer reported process-exit allocations rooted in system fontconfig/GTK libraries. The same ASan/UBSan test passed with leak detection disabled; this is consistent with other Qt UI sanitizer runs recorded above.

Control-state styling stage (2026-10-22):

- Dark and light themes now give disabled slider handles an explicit muted track color and border, so an unavailable seek control is visibly distinct from a focused/hovered one while retaining the existing opacity treatment.
- `code-size`, `appearance-preferences`, and `preferences-dialog` passed **3/3**. The updated appearance suite passed **9/9** under ASan/UBSan with leak detection disabled. Window behavior tests were not run; native Wayland focus-ring contrast remains desktop verification work.
- Formatting and `git diff --check` passed. No playback, decoding, or GPU rendering code changed.

Responsive transport stage (2026-10-22):

- TransportBar now selects compact metrics from its live width. At 560 px it keeps time and volume controls with smaller footprints; below 480 px it hides the optional time label and shrinks volume; below 360 px it hides volume as a final narrow-surface fallback. Button sizing, timeline interaction, and shared QAction wiring are unchanged.
- `code-size`, `appearance-preferences`, `preferences-dialog`, `action-feedback`, and `playback-overlay-text` passed **5/5**. The appearance suite covers metric calculations and live widget visibility at 400/560 px. ASan/UBSan appearance run passed **9/9** with leak detection disabled; enabling LeakSanitizer reports platform fontconfig/GTK allocations after Qt shutdown, not project-owned objects.
- Formatting and `git diff --check` passed. Window behavior tests were not run; native Wayland sizing, icon rasterization, and fullscreen pointer interaction remain desktop verification work. No libmpv, decoder, or GPU render-path code changed.

Action feedback compactness stage (2026-10-22):

- Action feedback geometry now clamps both the panel and its text area to the overlay viewport. Normal surfaces retain the centered, lower-third presentation; tiny fullscreen surfaces collapse padding instead of painting outside the video.
- `action-feedback` passed **8/8** in Debug and ASan/UBSan builds. The new pure geometry checks cover a 20×12 viewport and the normal 1920×1080 placement. Window behavior targets were not run; native Wayland visual fade and text rendering remain desktop verification work.
- Formatting and `git diff --check` passed. This is a UI-only change with no playback, decoder, or GPU render-path impact.

Playback overlay boundary stage (2026-10-22):

- Extracted overlay panel placement into a small geometry helper. Information and seek panels retain the 20/32 px breathing room at normal sizes, while their extents and insets contract for tiny viewports so fullscreen overlays remain fully contained and the seek panel stays centered.
- `playback-overlay-text`, `appearance-preferences`, `preferences-dialog`, `media-information`, and `code-size` passed **5/5**. The focused sanitizer run of `playback-overlay-text` passed **20/20** without ASan/UBSan reports. Window behavior targets were compiled but not run as requested; native Wayland visual centering and fade behavior remain desktop verification work.
- `clang-format --dry-run --Werror` and `git diff --check` passed. The helper and production arrangement do not alter libmpv GPU rendering, hardware decoding, or video geometry.

Content-first UI refinement stage (2026-10-22):

- Dark and light themes now share deliberate hover, pressed, checked, focus, and disabled feedback for controls; playlist-specific row styling is scoped to `playlistView`, and timeline seek handles have a more legible hover target. Playlist row striping no longer competes with selection styling.
- Transport icon buttons are keyboard-focusable and expose their current QAction text as a live tooltip and accessible name. High-frequency file/network, playlist, seek, volume, and information actions now use Qt standard icons and status tips; playlist editing actions have directional icons and the list has an accessible name.
- Fullscreen entry reveals the floating controls immediately and starts the existing fade timer, without changing video layout or renderer behavior. Runtime fullscreen action text and icon track the window state.
- Full Debug build passed, including compile-only window behavior suites. `appearance-preferences` and `code-size` passed; window behavior tests were compiled but not run as requested. `git diff --check` passed. No CPU frame copies, renderer changes, or polling were introduced.

UI feedback and accessibility polish stage (2026-10-22):

- The timeline now exposes and updates a `position / duration` tooltip and accessible description for backend updates, keyboard/drag changes, and pointer hover. Tooltip updates are value-coalesced and disappear when the pointer leaves the slider; no preview decoder or frame copy is involved yet.
- Network, playback, subtitle, and preference controls now expose explicit accessible names. Media information copy has a standard icon, tooltip, accessible name, and a copied confirmation tooltip.
- Debug build passed. `appearance-preferences`, `preferences-dialog`, `media-information`, and `code-size` passed; all window behavior targets were compiled but not run as requested. Formatting and `git diff --check` passed. No libmpv rendering or playback path changed.

Folder drag-and-drop stage (2026-10-22):

- Local drag routing now distinguishes files, subtitles, and one or more folders before emitting any side effect. Folder batches are scanned by the existing QtConcurrent worker and emitted as one naturally sorted result; mixed folder/media/subtitle drops are rejected atomically.
- `file-drop` passed with single-folder, multi-folder, mixed-drop, subtitle, and media regressions. Existing `folder-scan` also passed. Window behavior tests were compiled but not run as requested, so native Wayland drag delivery and visual scan feedback remain desktop verification work.
- No playback polling, CPU frame copies, or renderer changes were introduced; folder traversal remains off the GUI thread.

HDR detection and GPU tone-mapping stage (2026-10-22):

- libmpv media properties now identify PQ/HLG transfer functions and video primaries; the Tab overlay and media report expose HDR color information.
- The Video menu exposes `auto`, `bt.2390`, and `clip` tone-mapping intents, submitted asynchronously to mpv/libplacebo without CPU frame copies. A headless real-libmpv test verifies the property changes and accepted controller state.
- Full Debug build passed. `playback-overlay-text`, `media-information`, and `video-color` passed. Window behavior tests were compiled but skipped as requested. This stage does not establish native Wayland HDR output, monitor HDR metadata, compositor color management, or hardware-specific presentation.

Deinterlace control stage (2026-10-22):
- The Video menu exposes `off`, `auto`, and `on`, mapped directly to libmpv's `deinterlace` property without adding a CPU video-filter path.
- The real headless libmpv `video-color` test verifies the default `auto` state and the `yes`/`no` property transitions. The menu also re-translates BT.2390 and deinterlace actions during runtime language changes.
- `clang-format --dry-run --Werror`, `git diff --check`, and the code-size check are required before commit; no new source exceeds 500 lines.

Appearance and language settings stage (2026-10-22):

- Added persisted Dark, Light, and Follow System themes with immediate palette/style updates, plus a Settings → About dialog showing NekoPlayer, Qt runtime, and libmpv client API versions (no license claim).
- Added persisted Simplified Chinese/English selection. English can be switched at runtime; `--no-config` keeps reading/writing appearance preferences disabled. The bundled Qt Linguist catalog currently translates the core menus, settings, playback information overlay, and transport labels (**89 strings**); legacy dialogs and less common messages may remain Chinese and are follow-up localization work.
- Follow-up runtime-language stage: `LanguageManager` replaces the application-owned `QTranslator` without restarting playback and persists successful menu changes; the main window refreshes its primary menu labels and transport actions on `LanguageChange`. `language-manager` verifies English install, Chinese removal, and unsupported-language rejection. The broader catalog remains partial, and legacy dialogs/specialized submenu actions still need their own retranslation coverage. HDR is recorded as a separate graphics stage requiring mpv/libplacebo and Wayland display-path capability/fallback verification; no HDR output behavior is claimed here.
- `appearance-preferences` verifies all three theme modes and loads the bundled `.qm` resource. Full Debug build and `QT_QPA_PLATFORM=offscreen ./build/debug/NekoPlayer --no-config --smoke-test` passed. The non-window collection passed **44/44** before the last localized-resource test addition, and that updated `appearance-preferences` test passed again afterward. No window behavior test was run.
- `clang-format --dry-run --Werror`, code-size checks, and `git diff --check` passed; no new source exceeds 500 lines.

Runtime language refresh follow-up (2026-10-22):
- Expanded the English catalog for high-frequency action feedback, empty/error playback states, media reports, playlist editing, overlay accessibility labels, transport accessibility/tooltips and the media information dialog.
- On `LanguageChange`, the empty-state hint, playback/seek overlays, transport labels/tooltips, playlist editing actions, and an open media-information dialog refresh their translated content without replacing playback state or shared QAction objects. Main-window primary menu/action labels continue to refresh in place.
- Full Debug build and the non-window test collection passed **45/45**. The catalog/resource check `language-manager` passed; UI behavior targets were built but not run per request. Nested legacy menus still need broader retranslation coverage, and visible feedback already on screen is not reconstructed in the new language.
- Playback-control menus now also retranslate in place: navigation, chapters, A–B loop, speed/delay tuning and playlist repeat/shuffle. Dynamic titles are recomputed from the existing controller state while QAction data, shortcuts and check states remain intact. Focused `ab-loop`, `navigation`, `tuning`, `playlist-modes`, `language-manager` and `code-size` checks passed; the two unrelated session/framing tests passed on immediate focused rerun after one full-suite timing failure.

Video framing, playback overlay, and timeline stage (2026-10-22):

- Added PotPlayer-style video zoom/pan (25–800%, horizontal/vertical offsets, presets, keyboard shortcuts and reset) using mpv's native `video-zoom`/`video-pan-*` properties. Runtime intent survives stop/reload and renderer recovery; Stretch mode disables framing operations while preserving the values for later use.
- The Tab overlay now reports container and byte size, progress/remaining duration, volume/mute, audio/subtitle delays, sample format and selected audio/subtitle tracks. Track metadata is bounded and sanitized; network credentials/query secrets and external subtitle paths are never displayed.
- Fullscreen controls now float over the video with a fade and no layout contribution; the playlist hides while fullscreen and restores its checked preference on exit. Timeline track clicks seek immediately to the mapped location, while handle dragging and keyboard step behavior remain available.
- Full Debug build completed. The non-window suite `ctest --test-dir build/debug --output-on-failure -E 'window|startup'` passed **43/43** in **61.99 seconds**. `video-framing`, `playback-overlay-text`, and `seek-slider-geometry` are included. Window behavior tests were compiled but not run as requested; fullscreen geometry/fade, physical pointer interaction and actual video centering still need native-Wayland visual verification.
- Changed UI sources and tests were formatted; `git diff --check` passed. No new >500-line source was added; specialized tests remain below 800 lines. No new polling, GPU readback, or video-frame copy was introduced.

External audio tracks stage (2026-10-22):

- Added shared Audio/context-menu loading with Ctrl+Shift+A, including silent video and multi-stream audio containers. Loading uses asynchronous native `audio-add`; audio and subtitle attachments share typed loading/restoration logic. Local media preferences retain external paths, the selected stream ordinal, and explicit disabled/internal selection across reopening. Renderer recovery restores each canonical external file once, including containers with both audio and subtitle streams. Source changes while the chooser is open invalidate its result.
- Full Debug and relevant ASan/UBSan builds passed, including the application and compile-only desktop targets. The complete non-window Debug collection passed **40/40** in **56.65 seconds**. Sanitizer `code-size|backend-startup|external-audio|external-audio-session|track-preference-store|track-preference-session|tracks` passed **7/7** in **13.67 seconds**, without sanitizer reports.
- Before implementation, persistence regressions reproduced lost audio paths, malformed-field acceptance and stale disabled-selection intent. Review then identified mixed-container duplication: both native add commands import all stream types. Two real-libmpv regressions reproduced four external audio streams instead of two; canonical path-only deduplication now passes recovery and repeated preference restoration with both selected types preserved. Tests also cover second-stream selection, missing files, silent video, disabled/internal selection and repeated renderer resets. The disabled-recovery test waits for the observed track snapshot before resetting, matching the controller's snapshot contract.
- Independent follow-up review found no remaining Critical or Important issues. Changed-source formatting, diff and code-size checks passed; no new file-size exception was added. No window behavior tests were run. Actual Wayland chooser/shortcut interaction, physical audio output and synchronization remain unverified. This stage adds no periodic playback polling, render-path changes or CPU video-frame copies.

Current media utilities stage (2026-10-22):

- Added a shared File/context submenu for copying the complete local path or network URL (Ctrl+Alt+C), copying the current playback time including milliseconds (Ctrl+Shift+C), and opening a local media directory through QDesktopServices. Actions recheck active state when triggered; the folder action is disabled for network sources. Clipboard copying retains full signed URLs; the actions introduce no shell command, filesystem probing on progress events, playback polling or decoder/render changes.
- Full Debug build and ASan/UBSan NekoPlayer build passed. Debug `code-size|backend-startup|playback-time|action-feedback` passed **4/4** in **0.82 seconds**; sanitizer `code-size|backend-startup|playback-time` passed **3/3** in **1.07 seconds**, without reports. These existing non-window checks cover the reused time formatting and startup, not clipboard, menu wiring or file-manager launching. No window behavior tests were run and no additional tests were added for this desktop forwarding logic. Real Wayland clipboard and desktop-handler integration remains unverified.
- Read-only review found no issues; changed-source formatting and diff checks passed. The new menu implementation/header are 69/27 lines, with no size exception.

Recent media stage (2026-10-22):

- File and context menus share a recent-media submenu. Only successful file-loaded events update its newest-first, normalized, distinct 20-source list. Ctrl+Shift+T reopens the most recent source through the existing validated opening path. Clearing is independent of queue and resume history. Labels escape menu mnemonics; tooltips omit network user info, queries and fragments while stored signed URLs remain usable.
- Independent versioned `<session path>.recent.json` persistence uses bounded validation and QSaveFile at normal session save, with no per-position/per-file disk writes. Empty configuration paths stay memory-only. Invalid, future, oversized and externally corrupted documents are preserved and reported. Concurrent instances do not merge history or hold a cross-process lock.
- The headless suite first failed against a no-op implementation (24 failing cases), then passed after implementation. It checks ordering, duplicate movement, eviction, source normalization, signed URL round trips, explicit-save behavior, independent clearing, invalid-file preservation, external corruption, failed writes and memory-only mode. Full Debug build passed, including existing compile-only desktop targets; the complete non-window collection passed **38/38** in **51.68 seconds**. ASan/UBSan builds of NekoPlayer and the history suite passed; `code-size|backend-startup|recent-media-history` passed **3/3** in **1.07 seconds**, without sanitizer reports.
- Read-only review found no issues. Changed-source formatting and `git diff --check` passed; all new code files remain under 500 lines. No window behavior tests were run. Actual Wayland menu, shortcut and tooltip presentation remains unverified; this stage does not alter rendering or decoding.

Extended seek shortcuts stage (2026-10-22):

- Added shared Navigation menu/context-menu actions for Shift+Left/Right (30 seconds), Ctrl+Shift+Left/Right (5 minutes), and Home (current media beginning). Existing plain-arrow seeking and Ctrl+arrow frame stepping retain their bindings. New actions require active, seekable playback; Home does not autorepeat. All new actions use the existing typed controller APIs and fullscreen seek-progress feedback. Seeking beyond the end follows native EOF and queue behavior.
- Full Debug build succeeded. The selected non-window `code-size|action-feedback|playback-overlay-text|navigation|frame-step|playback` checks passed **6/6** in **10.47 seconds**. ASan/UBSan builds of NekoPlayer, `frame_step_window_test` and `fullscreen_feedback_window_test` succeeded. Independent read-only review found no issues; changed-source formatting and `git diff --check` passed, with no code-size exception.
- No window behavior tests were run. The existing headless tests check the reused backend and text/progress formatting, not physical shortcut dispatch or the new menu-to-progress wiring. Those Wayland interactions remain unverified. This bounded UI addition reuses the existing backend command implementation and introduces no polling, frame copies or rendering resources.

Asynchronous media folder import stage (2026-10-22):

- Added a shared folder submenu to File, Playlist and the context menu. Ctrl+D replaces the queue and Ctrl+Shift+D appends; optional subfolder traversal defaults off. QtConcurrent performs extension filtering and numeric filename sorting away from the GUI thread. Hidden entries and symlinks are skipped, and all encountered entries count toward a 100000-entry traversal limit. Empty, failed, cancelled and superseded results leave the existing queue intact.
- Complete Debug build succeeded, including the compile-only `folder_window_test`. The complete non-window Debug collection passed **37/37** in **52.92 seconds**. After formatting and removing the timing-sensitive test assertion, the incremental Debug build and focused `code-size|folder-scan` selection passed **2/2** in **2.86 seconds**. The application, folder worker and desktop folder test targets also built under ASan/UBSan; the final targeted `code-size|backend-startup|folder-scan` selection passed **3/3** in **6.47 seconds**, with no sanitizer reports.
- The folder suite covers filtering, case-insensitive extensions, natural ordering, recursion, hidden/symlink entries, invalid/unreadable roots, embedded NUL, the traversal limit, cancellation, stale result/error suppression, GUI-thread delivery, object deletion and reentrant replacement/deletion from busy notifications. Initial missing-feature failures were observed before implementation. The large-folder destruction test intentionally makes no timing-dependent claim that its worker is still running when the QObject is deleted.
- Scan jobs capture paths and a shared atomic cancellation flag, without UI/player pointers. Generation checks suppress stale results before and after synchronous state notifications. Source/queue changes, manual item activation, explicit stop/previous/next and close invalidate pending imports. Cancellation is cooperative: an operating-system filesystem call and the bounded final sort can delay worker termination, while deleting its QObject does not wait for the worker.
- No window behavior tests were run. Shared actions, queue integration and folder-dialog interaction have compile-only coverage; real Wayland shortcut delivery and dialog behavior remain unverified. No playback polling, decoder changes or CPU frame copies were introduced. Local Haruna, KikoPlay and mpv.net references informed behavior; no upstream source was copied.
- Independent review found no Critical or Important issues. Its timing-sensitive test concern was addressed before the final focused checks. Changed-source formatting, `git diff --check` and code-size checks passed; all new code files remain below 500 lines.

Native screenshot stage (2026-10-22):

- Full Debug build succeeded, including compile-only desktop targets. `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **36/36** in **51.17 seconds**.
- Added shared Video/context-menu screenshot actions: Ctrl+E includes current subtitles; Ctrl+Alt+E requests video only. The save chooser provides PNG/JPEG filters, default suffixes, timestamp names and normal overwrite confirmation. Unsupported explicit suffixes and playback changes while choosing a path receive visible feedback. Cancellation performs no write.
- The controller validates absolute, NUL-free PNG/JPG/JPEG targets and active decoded video, rejects duplicate in-flight work, and uses literal argument-array `screenshot-to-file` commands. Only the matching command reply publishes completion; pending work retains ownership across stop/load. Native errors do not become media-load failures. Existing files may be overwritten after confirmation; screenshot writes are not claimed to be atomic.
- The focused red case failed against the empty implementation. The final real-libmpv suite passed **10 QtTest cases** under ASan/UBSan, checking actual image format/dimensions, Unicode/spaces/property-like filenames, validation, failure/retry, eligibility and stop lifecycle. A paused-frame pixel comparison covers subtitle inclusion and a subtitle-disabled baseline catches inverted flags. The first subtitle fixture assumed automatic selection; it was corrected to explicitly select the fixture's track, without changing production track behavior.
- The relevant sanitizer targets, application and compile-only video window suite built successfully. `ctest --preset sanitizers -R '^(code-size|screenshot|backend-startup)$'` passed **3/3** in **3.40 seconds**, without sanitizer reports. Independent review found no must-fix issues. Changed-source formatting and diff checks passed; screenshot backend/UI/test files are 43/86/221 lines, with no new size exception.
- No window behavior tests were run. The native encoder and subtitle pixels were exercised with null video output; actual Wayland dialog/shortcut behavior, GPU screenshot readback, HDR screenshot conversion and desktop composition remain unverified. Normal playback keeps its GPU render path; image encoding/readback is requested only for an explicit screenshot, with one in flight.

Playback information and independent seek overlay stage (2026-10-22):

- Tab toggles a persistent plaintext information panel. Left/right actions and transport seeking show a separate progress panel for 1.8 seconds; repeated seeks restart its timer. Information remains independent of the timer and fullscreen transport visibility. Progress uses observed mpv position, with finite/seekable/positive-duration guards. Metadata strings are bounded and control-cleaned; network titles omit authentication and query data.
- Full Debug build succeeded, including the updated compile-only fullscreen test. The relevant ASan/UBSan targets, application and fullscreen test also built. Debug selection `^(code-size|playback-overlay-text|action-feedback|media-information|media-report|playback-time)$` passed **6/6** in **3.45 seconds**; the same sanitizer selection passed **6/6** in **3.92 seconds** without sanitizer reports. The formatter suite contains 15 QtTest cases; its implementer reproduced four missing-feature failures before implementation.
- Independent review found no blocking defects. New window tests cover action wiring and fullscreen progress independently of transport, but were deliberately not run. They trigger QActions directly; physical Tab/arrow dispatch and actual Wayland overlay composition remain unverified. No polling, render-path changes or CPU frame copies were introduced. Formatting and diff checks passed after formatting the new formatter/test sources; no code-size exception was added.

Playlist source validation stage (2026-10-22):

- M3U8 import previously checked URL syntax without applying the playback source policy, and export could overwrite a playlist with unsupported or malformed sources. Both paths now use `MediaSource` validation. Import resolves relative paths against the playlist directory, converts file URLs to absolute local paths, and preserves network spelling and signed escapes. Any invalid entry rejects the whole import before publishing entries; export validates before opening `QSaveFile`, preserving an existing file on validation failure.
- The initial regression run reproduced 13 failures before implementation. The final `m3u8_store_test` passed **26 QtTest cases**, covering hostless/unsupported URLs, invalid ports/escapes, encoded NUL, relative file URLs, file queries, duplicate-row selection and signed network round trips.
- `cmake --build --preset debug -j 4` succeeded, including compile-only desktop targets. The final `ctest --test-dir build/debug -R '^(code-size|m3u8-store|media-source|network-session|network-playback)$' --output-on-failure` passed **5/5** in **3.36 seconds**. The corresponding sanitizer targets built successfully and the same selection under `build/sanitizers` passed **5/5** in **3.87 seconds**, without sanitizer reports.
- Independent review reported no new must-fix findings. It noted a pre-existing edge: scheme-less strings beginning with `://` are treated as relative local paths by the playlist loader. This stage does not change relative-path interpretation. `git diff --check`, changed-source `clang-format --dry-run --Werror`, and the 500/800-line checker passed; the store and its test are 116 and 218 lines, respectively.
- No window behavior tests were run. Validation performs string processing without network or file-existence probes; this stage does not establish public network protocol interoperability, physical audio, GPU decoding, HDR or rendered Wayland video.

Network media opening stage (2026-10-22):

- Added shared source validation/normalization, a Ctrl+U network address dialog, shared File/context-menu action, and mixed local/network command-line queues. Accepted network schemes are HTTP(S), RTSP(S) and RTMP(S). Whole-batch validation precedes queue replacement; signed path/query escape bytes survive normalization. Network items persist in the queue, restore idle, and do not contribute local history, track fingerprints, chooser directories or persisted resume positions. Renderer recovery preserves an in-memory position only for seekable network media.
- Full Debug and ASan/UBSan preset builds succeeded, including the compile-only desktop targets; a final build check reported no pending work. The final solo Debug collection using `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **34/34** (47.00 seconds summed test durations, complete log ending at 17:24 CST). The same selection under `build/sanitizers` passed **34/34** in **65.70 seconds**, with leak detection enabled and no sanitizer reports. The original Debug process handle was unavailable after continuation; its complete CTest log was inspected and all 34 test outcomes counted. Window/desktop behavior suites were built but not run.
- An earlier Debug collection passed 32/34: frame-step failed its expected-position assertion and A–B looping timed out waiting for a reentrant load, then threw on an empty optional. Both passed a focused rerun and five repeated runs each; the final solo collection above also passed. Their cause is not established, and concurrent sanitizer execution during the failed collection is an observation rather than a diagnosis. No unrelated production or test behavior was changed to hide these failures.
- The new GUILESS integration suite uses real libmpv and a bounded loopback HTTP server to verify actual signed/Unicode request bytes, pause/seek/resume/stop, HTTP 404 fallback to a local queue item, and no request from restored queues until explicit Play. Source/session suites cover malformed URLs, file URLs, duplicate ownership after invalid-item filtering, and discarded network bookmarks. Renderer-recovery regressions cover nonseekable RTSP, seekable HTTP and a pending local bookmark.
- Red/green checks reproduced escape canonicalization, incorrect nonseekable recovery position, and decoded NUL file paths before their fixes. Independent review found that encoded NUL could break normalization idempotency and permit queue clearing before revalidation; the decoded local path now rejects NUL, and both source and persisted-session regressions passed. Follow-up review found no remaining Critical or Important findings. `git diff --check`, changed-source `clang-format --dry-run --Werror`, and the 500/800-line checker passed; no new size exception was introduced.
- Public HTTPS, HLS, RTSP/RTMP and webpage/yt-dlp playback were not separately exercised. Protocol validation and synthetic recovery state do not prove these services work. No automated Wayland/window behavior, physical audio, GPU/hardware-decoding or HDR validation was performed in this stage. The application was subsequently launched with `QT_QPA_PLATFORM=wayland` at the user's request, and niri reported a NekoPlayer window; that observation alone does not establish video rendering. Network I/O remains in mpv; the Qt Network dependency is test-only, with no new playback polling or CPU frame-copy path.

Media track preference persistence stage (2026-10-22):

- Added asynchronous per-media restoration for internal audio/subtitle tracks, explicit disabled selections, and external subtitle streams. Track choices are stored separately from the session and playback-history files, keyed by normalized path plus size/mtime fingerprint; missing external subtitles are reported without blocking playback.
- The regression suite covers controller restart, asynchronous external subtitle loading, multi-stream external subtitles, final observed selection at completion, renderer recovery without duplicate external tracks, explicit audio/subtitle off selections, missing/broken subtitles, stale/corrupt configuration, and file changes both between loads and during playback. A media-switch regression first failed because requested track IDs remained attached to the old path, then passed after path/fingerprint lifecycle isolation. User selection, pending open, stop, and reentrant signal callbacks invalidate stale restoration.
- Full Debug and ASan/UBSan preset builds succeeded, including compile-only desktop targets. `ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` passed **31/31** in **44.48 seconds**. The same selection under `build/sanitizers` passed **31/31** in **60.09 seconds**, with no sanitizer reports and leak detection enabled.
- The exclusion expression skips 13 of 44 registered tests: every name containing `window`, plus `file-startup`, `preferences-dialog`, and `video-color-dialog`. These window/desktop tests were compiled but not executed, as requested.
- Broad regression exposed a decoded-audio metadata refresh gap when mpv reused the audio output: an unchanged `audio-params` observation could leave the controller sample rate at zero. Playback restart now schedules an asynchronous refresh; the replacement-media test checks the preceding sample rate and replacement format/channels. Sanitizer metadata tests also passed eight repeated runs. The video-presentation queue-saturation test now consumes an actual property reply, rather than a synthesized observation that releases no queue slot; its sanitizer suite passed 20 repeated runs. No production aspect-setting behavior changed.
- An initial full Debug run failed the subtitle-drop pause prerequisite: `track-list` can arrive before `FILE_LOADED`, while `setPaused` requires active playback. That test now waits for active playback before requesting pause and retains all post-drop pause, position, queue and no-reload assertions. The final full collections above include this correction. A subtitle test also stopped retaining an iterator across event processing that could replace the track snapshot.
- `git diff --check`, `clang-format --dry-run --Werror` on all touched/new C++ sources, and the 500/800-line checker passed. The new restore implementation is 206 lines, SessionManager is 319, and the session/controller subtitle suites are 407/396; no new size exception was introduced. Independent review prompted strict malformed-entry preservation, active-file fingerprint capture, final-snapshot publication, and cancellation reentrancy fixes; follow-up review reported no remaining must-fix findings.
- These headless tests do not establish actual Wayland window behavior, subtitle pixels/fonts, physical audio output, GPU hardware decoding or HDR. The QOpenGLWidget/libmpv GPU render path and hardware-decoding policy remain unchanged; no periodic playback polling or CPU frame copies were added. Crash recovery, content hashes, network-media preferences and multi-instance merging remain outside this stage.

Video color adjustment stage (2026-10-22):

- Added brightness, contrast, saturation, hue and gamma adjustment through mpv's native video properties, with finite-value normalization and the documented -100 to 100 range. The modal dialog previews changes, treats Apply as the new Cancel rollback point, and restores that point on Cancel. Values are saved in the session configuration and restored before subsequent playback.
- `cmake --build --preset debug -j 4` succeeded. The complete non-window Debug CTest set passed 29/29 in 43.32 seconds, including backend startup, session and video-color integration. The video-color dialog test target compiled but was deliberately not run; window behavior tests were skipped at the user's request.
- `cmake --build --preset sanitizers --target video_color_test session_test -j 4` succeeded, and the ASan/UBSan CTest selection `video-color|session` passed 2/2 in 1.25 seconds without sanitizer reports. `git diff --check`, `clang-format --dry-run --Werror` on touched C++ files, and the 500/800-line checker passed.
- This stage does not establish rendered color output, HDR behavior, Wayland presentation, or hardware-decoder interoperability; the normal libmpv GPU render path and hardware-decoding preference are unchanged, and no decoded frames are copied to CPU memory. The dialog test target compiled but was deliberately not run; window behavior tests were skipped at the user's request.

Unified preferences dialog stage (2026-10-22):

- Added one modal preferences window with playback, audio and subtitle categories. F5 opens the playback category and F6 opens subtitles; audio output remains an immediate action while decoding and subtitle settings use Apply/OK.
- `cmake --build --preset debug -j 4` succeeded. The selected non-window set excluding `file-drop` and `preferences-dialog` passed 28/28 in 45.33 seconds. An initial `file-drop` run failed three pause assertions because the test could observe `track-list` before `FILE_LOADED`, then request pause while the player was still inactive. The test now waits for active playback before pausing; the nine invalid-drop data cases and complete `file-drop` suite passed. `window`, `preferences-dialog` and other window behavior tests were not run, per the user's request.
- `clang-format --dry-run --Werror` passed for the changed C++ files; `git diff --check` and `cmake -DNEKO_SOURCE_DIR="$PWD" -P cmake/CheckCodeSize.cmake` passed. New dialog sources are 206 and 41 lines; its test is 36 lines.
- The dialog test is compile-only in this stage. Interactive category navigation, Apply/Cancel behavior, real audio-device selection and Wayland rendering/behavior remain unverified.

Audio output device selection stage (2026-10-22):

- The controller parses `audio-device-list` into owned Qt snapshots, asynchronously applies a selected device, and leaves the observed selection unchanged when the backend rejects a request. Session preferences retain an unavailable device ID while using `auto` at runtime, then reapply it when it returns. Menu-bar and context-menu entries share one `AudioDeviceMenu`.
- `cmake --build --preset debug -j4` succeeded, including NekoPlayer and compile-only window targets. `ctest --test-dir build/debug -R '^(code-size|audio-device|playback|session)$' --output-on-failure` passed 4/4 in 4.09 seconds.
- `cmake --build --preset sanitizers --target audio_device_test playback_test session_test -j4` succeeded. `ctest --test-dir build/sanitizers -R '^(audio-device|playback|session)$' --output-on-failure` passed 3/3 in 4.60 seconds without sanitizer errors. `clang-format --dry-run --Werror`, `git diff --check` and `cmake -DNEKO_SOURCE_DIR="$PWD" -P cmake/CheckCodeSize.cmake` passed.
- `window_test` was compiled but not run. No GUI/Wayland interaction, physical output-device switching, or audible audio-quality test was performed; null audio integration is not evidence of real device output.

Pending-open state and event ordering stage (2026-10-22):

- A successful asynchronous `loadfile` request now exposes its target as pending playback immediately, while hiding metadata, tracks, chapters, A–B endpoints and frame-step capability from the previous file. A rejected command restores the previous playback path and media state.
- Added regression coverage for `START_FILE`, `FILE_LOADED`, `END_FILE` and command replies arriving in either order, stale-generation END_FILE reasons, and synchronous signal handlers that re-enter with a newer open. Terminal notifications from an older generation are suppressed after reentrancy.
- `cmake --build --preset debug -j 4` succeeded. The selected non-window CTest set passed 27/27 in 43.88 seconds. Window interaction tests were skipped as requested.
- The ASan/UBSan targets for open failure/order, media information, frame stepping, A–B looping, playback and navigation built successfully; the selected sanitizer CTest set passed 6/6 in 16.50 seconds without sanitizer errors.
- `clang-format --dry-run --Werror`, `git diff --check`, and the 500/800-line check passed. The touched controller and test files remain below 500 lines.
- No interactive Wayland rendering, shortcut delivery, or physical hardware-decoding test was run.

Environment: native Linux Wayland session (niri), Qt 6.12.0, system libmpv 0.41.0 and GCC 16.2.1. The verified display is one 2880x1800 panel at 1.75 scale. Window tests use the Qt Wayland platform plugin, null audio and `MALLOC_PERTURB_=128`.

Static analysis review (2026-10-22):

- `clang-tidy` 23.1.3 processed all 40 project C++ translation units with a separate Clang 23 compile database using `clang-analyzer-*`, `bugprone-*`, and `performance-*`. The initial GCC 16 compile database was incompatible with clang-tidy (`-mno-direct-extern-access`), so it was not treated as analysis evidence.
- No clang-analyzer diagnostics were reported. Reviewable warnings include Qt container `qsizetype` to model/config `int` conversions, adjacent arguments that clang-tidy considers easy to transpose, a conservative integer-division precision warning, and small copy/layout suggestions. The integer division intentionally reserves rounding headroom; Qt value copies are implicitly shared, and reordering the one-per-player private implementation object has negligible measured value. Queue/model row counts are practically bounded by available memory, but explicit conversion bounds remain a potential hardening item.
- Read-only callback review found the renderer and libmpv wakeup callback teardown ordering synchronized and lifetime-safe. Path conversion remains `QString` to UTF-8 at libmpv boundaries; volume inputs and persisted values are finite and clamped to 0–100. At the time of this review, a concrete open-failure edge remained: synchronous `loadfile` submission failure or an asynchronous command-reply error could leave the requested path in state without emitting `mediaLoadFailed`.
- Follow-up in the pending-open state stage closed that controller defect: rejected asynchronous replies and synchronous submission failures restore the previous path and emit `mediaLoadFailed`; `mpv_open_failure_test` covers both paths while preserving existing playback state. The tests assert the controller signal and rollback, but do not inject a rejection through `PlaylistController` to verify queue advancement end to end.
- The scan was read-only. No source changes or GUI/window tests were made; this does not establish Wayland rendering, real hardware decoding, or interactive desktop behavior.

Session resume position stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and compile-only window suites. The selected headless CTest run passed 21/21 in 38.40 seconds: code-size, backend startup, resume, media information/report, frame stepping, A-B looping, decoding, playback, playlist editing/modes/shuffle, tracks, session, tuning, navigation, playback time, file drop and video presentation.
- Sanitizer targets rebuilt successfully. The selected ASan/UBSan CTest run passed 9/9 in 24.75 seconds: media information, navigation, playback, playlist/edit/modes/shuffle, resume and session, without sanitizer errors.
- Real headless libmpv coverage verifies idle and loading saves, native `loadfile start=` resume without autoplay, one-time consumption, near-end and stop reset, duplicate-path identity, persistent-index moves, missing/undecodable selected entries, rapid replacement ownership, renderer recovery before the first position event, and native single-file repeat returning to media start. No polling, CPU frame copies, Rust or Vulkan path was added; GPU OpenGL rendering and `hwdec=auto-safe` remain unchanged.
- Initial regressions were reproduced before the final fixes: a failed selected file with no successor retained its old bookmark, a rapid replacement let an earlier END_FILE clear a newer load's offset, and renderer recovery before the first position event restarted at zero. Bookmark ownership now follows libmpv command replies and `playlist_entry_id`, and recovery transfers the pending target to its new load.
- Formatting, `git diff --check`, the 500/800-line checker and clang-format dry-run passed. `tests/window_test.cpp` remains the existing 800-line exception at 566 lines; the new resume test is 345 lines and all other touched ordinary files remain under 500 lines.
- No GUI/window/smoke tests ran, per the user's request. Actual Wayland shortcut delivery, rendered frames, physical hardware decoding, and file identity validation after a path is replaced remain unverified or intentionally out of scope. Session saving remains normal-exit only and does not provide crash recovery or per-file history.

Media information stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|media-information|media-report|frame-step|ab-loop|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|playlist-shuffle|tracks|session|tuning|navigation|playback-time|file-drop|video-presentation)$' --output-on-failure` passed 20/20 in 36.38 seconds after the final production fix.
- `cmake --build --preset sanitizers --target media_information_test media_report_test playback_test navigation_test tracks_test frame_step_test ab_loop_test decoding_test -j 4` succeeded. `ctest --test-dir build/sanitizers -R '^(media-information|media-report|frame-step|ab-loop|decoding|navigation|playback|tracks)$' --output-on-failure` passed 8/8 in 20.65 seconds without sanitizer errors.
- The real headless metadata suite passed nine cases including setup/cleanup: actual video/audio codec and decoded parameters, exact source byte size, unavailable source size, pause/seek/resume without reload or metadata churn, audio-only replacement, stop/failure clearing, identical reload with delayed event draining, renderer recovery, disabled/restored tracks and reentrant stop/replacement callbacks. The pure report suite passed four cases including setup/cleanup, covering plain-text paths, actual hardware-decoder status, unknown values and inactive media.
- The missing snapshot initially failed on video width. Subsequent reload regressions exposed coalesced observations and decoder setup occurring after FILE_LOADED. Loading and audio/video reconfiguration now queue bounded asynchronous refreshes after the current events drain; stale requests are consumed without restoring old metadata. No periodic polling, CPU frame copies or hardware-decoding preference changes were introduced.
- Independent read-only review identified failed asynchronous GET replies retaining the requested format and zeroed storage. A real lavfi source reproduced unavailable size being reported as zero bytes before the fix. Failed replies now normalize to unavailable values, preserving size -1 and clearing stale parameters without reporting expected metadata absence as a playback error. The regression passed afterward; follow-up review reported no remaining Critical, Important or Minor findings.
- No GUI/window/smoke tests ran, per the user's request. The new compile-only window suite covers shared Ctrl+F1 actions, one reusable nonmodal read-only dialog, copy-button presence, stop/media updates and selection retention during progress. Actual Wayland shortcut delivery, dialog interaction, clipboard behavior, rendered frames and physical hardware decoding remain unverified. Renderer-recovery tests exercise backend reload logic without a GL context.
- Formatting, `git diff --check` and the 500/800-line checker passed. Event dispatch was extracted by responsibility to keep the private controller header at 352 lines; event handling is 186, metadata handling 120, dialog implementation 52, metadata tests 167, report tests 58 and window tests 61. No new size exception was needed.

Native frame-stepping stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all compile-only window suites. After the final lifecycle-ownership cleanup, `ctest --test-dir build/debug -R '^(code-size|backend-startup|frame-step|ab-loop|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|playlist-shuffle|tracks|session|tuning|navigation|playback-time|file-drop|video-presentation)$' --output-on-failure` passed 18/18 in 34.10 seconds.
- `cmake --build --preset sanitizers --target frame_step_test ab_loop_test navigation_test playback_test tracks_test -j 4` succeeded. After the final cleanup, `ctest --test-dir build/sanitizers -R '^(frame-step|ab-loop|navigation|playback|tracks)$' --output-on-failure` passed 5/5 in 16.85 seconds without sanitizer errors.
- The real headless libmpv suite covers paused 25 fps forward/backward steps at actual 40 ms intervals without reload, backward at zero, stepping during playback followed by pause/resume, idle/audio/still-image exclusion, immediate stop/replacement gating, failed loads, renderer recovery and identical reloads with delayed event draining. The absent slot failed first; overly broad active-media eligibility then failed against audio before the motion-video observation was implemented.
- A test initially captured time at the pause notification before native backward seeking finished. Event inspection reproduced pause followed by a later time update; the test now waits for backend seeking to settle and the controller to catch up before checking stable paused time. This query exists only in tests; production uses asynchronous native commands and property observations.
- Independent read-only review found no Critical/Important defects. Its minor lifecycle-ownership suggestion was implemented and reviewed again: generic media readiness resets in open/stop/reset, before notifications, rather than inside A–B cleanup. Selected-video-disabled and nonseekable-motion-video cases remain focused coverage gaps.
- No GUI/window/smoke tests ran, per the user's request. The compile-only window suite asserts shared menu/window actions, Ctrl+Left/Right shortcuts, no auto-repeat, native command dispatch, media-dependent availability, and existing subtitle shortcuts. Actual Wayland shortcut delivery, rendered frames, hardware decoding and backward precision on long GOP/VFR media remain unverified by this stage. The GPU renderer and `hwdec=auto-safe` are retained; no CPU frame copies, production polling or FPS-based seek estimates were added.
- Formatting and `git diff --check` passed. The 500/800-line checker passed without a new exception: the private controller header is 488 lines, navigation implementation 118, navigation menu 124, headless suite 156 and desktop suite 68.

A–B interval looping stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|ab-loop|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|playlist-shuffle|tracks|session|tuning|navigation|playback-time|file-drop|video-presentation)$' --output-on-failure` passed 17/17 in 30.77 seconds.
- `cmake --build --preset sanitizers --target ab_loop_test playback_test navigation_test playlist_test playlist_edit_test playlist_modes_test playlist_shuffle_test tracks_test -j 4` succeeded. `ctest --test-dir build/sanitizers -R '^(ab-loop|playback|navigation|playlist|playlist-edit|playlist-modes|playlist-shuffle|tracks)$' --output-on-failure` passed 8/8 in 26.22 seconds without sanitizer errors.
- Real headless libmpv checks cover accepted endpoints, paused edits without reload, actual native B-to-A playback, clear followed by EOF, invalid/nonfinite/reversed intervals, stop/replacement/failure clearing, renderer-reload retention and subsequent replacement reset, partial clear submission and retry, and whole-file repeat after clearing A–B. The initial no-op implementation failed all five original behavior cases before implementation.
- A removed temporary media file reproduced stale endpoints after renderer recovery failed. END_FILE now cancels recovery before resetting media on nonexpected errors. A full reply queue exposed a second failure path: rejected recovery `loadfile` generates no later file event, so immediate rejection now explicitly cancels recovery and clears endpoints before reporting it.
- Independent review identified synchronous error reentrancy during two-step cleanup. The regression failed with backend B unset while replacement media's accepted B was 2.5: the first error callback opened and configured new playback before the old second clear was submitted. Both old clears are now submitted before endpoint notifications or errors. The regression passes against actual backend endpoints; follow-up review reported no remaining Critical/Important findings.
- No GUI/window/smoke tests ran, per the user's request. The new compile-only window suite covers shared actions, registered shortcuts, media-dependent availability, endpoint edits, feedback, pause preservation and navigation-submenu placement. Actual Wayland shortcut/menu delivery and rendering remain unverified in this stage. Recovery tests exercise controller reload logic without a GL context. No progress polling, CPU frame copies or hardware-decoder changes were introduced.
- Formatting, `git diff --check` and the 500/800-line checker passed. No new exception was needed: the private controller header is 478 lines, loop implementation 63, menu implementation 68, headless suite 292 and desktop suite 67.

Shuffle playback stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|playlist-edit|playlist-modes|playlist-shuffle|session)$' --output-on-failure` passed 8/8 in 16.27 seconds.
- `cmake --build --preset sanitizers --target playback_test playlist_test playlist_edit_test playlist_modes_test playlist_shuffle_test session_test -j 4` succeeded. `ctest --test-dir build/sanitizers -R '^(playback|playlist|playlist-edit|playlist-modes|playlist-shuffle|session)$' --output-on-failure` passed 6/6 in 16.73 seconds without sanitizer errors.
- New headless tests cover independent shuffle/repeat settings, duplicate entries visited once per cycle, unchanged displayed order, actual previous/forward history across moves and removals, repeat-all cycle boundaries, append/clear/restart, paused toggles without reload or position loss, repeat-one manual navigation, real EOF, missing/undecodable files and reentrant error callbacks. A helper-level check performs 4096 draws and confirms history stays bounded at 1024. Missing shuffle behavior failed before implementation.
- Backward-failure regressions exposed three boundaries before their fixes: a fresh repeat-all cycle excluded the sole surviving current entry forever; removing forward history followed by an asynchronous backward failure caused a Qt list index assertion; and a failed Previous could hide surviving forward history. Fresh cycles now include every row while avoiding an immediate first-draw repeat, empty-history access checks cursor bounds, and exhausted failed backward loads rewind the history boundary. Synchronous missing-file scans preserve the still-playing entry's cursor.
- Review also found stale paused navigation actions after an exhausted missing-file scan. A regression failed before adding `navigationChanged`; manual exhaustion and queue completion now notify the window independently of time progress. Follow-up read-only review found no remaining Critical/Important issues and reran both reported regressions plus error-callback cancellation successfully.
- Shared shuffle QAction, Shift+R, independent checked state, mode title and navigation availability have compile-only window assertions. No GUI/window/smoke tests ran, per the user's request. Actual Wayland shortcuts, menu interaction, GPU rendering and active hardware decoding remain unverified by this stage. The OpenGL render path and safe hardware-decoding preference are unchanged; no playback polling or CPU frame copies were added.
- Formatting, `git diff --check` and the 500/800-line checker passed. No new size exception was needed: the playlist controller is 341 lines, shuffle helper 99 lines and new headless suite 384 lines.

Video presentation stage (2026-10-22):

- A final pre-commit rerun passed the same selected Debug checks 15/15 in 23.75 seconds and ASan/UBSan suites 13/13 in 27.37 seconds without sanitizer errors. No code changed between these runs and the builds below.
- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all five compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time|file-drop|video-presentation)$' --output-on-failure` passed 15/15 in 24.04 seconds.
- `cmake --build --preset sanitizers --target decoding_test subtitles_test playback_test playlist_test playlist_edit_test playlist_modes_test tracks_test session_test tuning_test navigation_test playback_time_test file_drop_test video_presentation_test -j 4` succeeded after the review fixes. `ctest --test-dir build/sanitizers -R '^(decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time|file-drop|video-presentation)$' --output-on-failure` passed 13/13 in 27.63 seconds without sanitizer errors.
- The new real headless libmpv suite reports 11 passes including setup/cleanup: four forced ratios from stretch, restoring source ratio, rapid mixed rotation with wraparound, negative/large quarter turns, invalid enums and unsupported angles including integer extremes, changes during paused playback with tracks/queue/position/hwdec retained, renderer recovery, stop/reopen, fresh-controller defaults and reply-queue exhaustion. The initial no-op implementation failed all eight original behavior cases. A subsequent timeout was an incorrect test expectation: mpv accepts aspect override `no` but reads it back as -2. The behavior was reproduced separately and checked against v0.41.0 `parse_double_aspect()`; the tests now use the numeric property API.
- Review found that partial enqueue of the two aspect setters could leave the backend inconsistent with accepted intent, while the equality guard prevented reselecting the prior ratio from restoring it. A real libmpv queue-exhaustion regression failed before the fix and passes after it: each selection now resubmits both setters, and the menu restores accepted checks after rejected submission. This is user-triggered recovery, not an atomic transaction or automatic retry. Follow-up read-only review reported no remaining findings. Formatting, `git diff --check` and the 500/800-line checks passed; no new size exception was needed.
- No GUI/window/smoke tests ran, per the user's request. The compile-only desktop suite covers shared menu placement, mutual aspect checks, rejected-action check restoration, direct controller updates, rotation actions, reset availability and registered shortcuts. Actual Wayland menu/shortcut delivery, aspect/rotation pixels, metadata display, fullscreen stretching and GPU/hardware-decoder interoperability remain unverified in this stage. The headless renderer-recovery case exercises the controller reload path only.

External subtitle drop stage (2026-10-22):

- `cmake --build --preset debug -j 4` succeeded, including NekoPlayer and all four compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time|file-drop)$' --output-on-failure` passed 14/14 in 22.79 seconds.
- `cmake --build --preset sanitizers --target decoding_test subtitles_test playback_test playlist_test playlist_edit_test playlist_modes_test tracks_test session_test tuning_test navigation_test playback_time_test file_drop_test -j 4` succeeded; the affected target was rebuilt after review fixes. `ctest --test-dir build/sanitizers -R '^(decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time|file-drop)$' --output-on-failure` passed 12/12 in 26.07 seconds without sanitizer errors. Formatting, `git diff --check` and 500/800-line checks passed; the new input implementation is 48 lines, headless suite 145 lines and desktop suite 64 lines.
- The headless drop suite runs 20 cases (plus setup/cleanup) against the real router and libmpv: media order with duplicates and unknown extensions, selected external subtitles with no media reload, paused position and queue preservation, Unicode/spaces/special characters, uppercase extensions, idle subtitle rejection, complete rejection of mixed/multiple/remote/invalid/empty drops, and asynchronous missing-subtitle errors without losing playback. The initial placeholder implementation failed all original 18 cases before implementation.
- Independent read-only review found that Qt considers `file://server/...` local and `QFileInfo` can reinterpret its UNC path. Two added regressions failed before the fix; the router now rejects all nonempty hosts, including localhost, before any side effects. Follow-up review found no remaining Critical/Important findings. README commands include the new suite and document the supported local URL spelling.
- No GUI/window/smoke tests ran, per the user's request. The compile-only desktop suite checks the MainWindow adapter, accepted/rejected events, error forwarding and subtitle-vs-queue routing. Native Wayland pointer drag/drop delivery, subtitle pixels and actual GPU/hardware-decoder behavior remain unverified in this stage. IDX/SUB companion decoding is delegated to mpv and was not exercised with a VobSub fixture.

Playlist item editing stage (2026-10-22):

- `cmake --build --preset debug` succeeded, including NekoPlayer and all three compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 13/13 in 20.12 seconds.
- `cmake --build --preset sanitizers --target decoding_test subtitles_test playback_test playlist_test playlist_edit_test playlist_modes_test tracks_test session_test tuning_test navigation_test playback_time_test` succeeded. `ctest --test-dir build/sanitizers -R '^(decoding|subtitles|playback|playlist|playlist-edit|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 11/11 in 23.23 seconds without sanitizer errors. Formatting, `git diff --check` and the 500/800-line checks passed. The new ordinary tests are 334 and 67 lines; no new size exception was needed.
- Qt model checks cover moves in both directions, persistent duplicate identity, invalid/no-op moves, same-index replacement notification, live internal drag data, and foreign/forged/removed source rejection. Headless libmpv checks cover editing paused playback at a known position without reload, saved order/current row, stopping and selecting a neighbor on removal, explicit restart, failure-history remapping, and ignoring EOF after removal. The initial five missing-behavior checks failed before implementation. The final playlist-edit executable reports 17 passed, including setup/cleanup and data rows.
- Review found a synchronous missing-file error callback could remove the current item, yet the old scan continued and opened a later item. Regressions reproduced the wrong current row and skipped surviving entry before the fix. Scans now snapshot remaining persistent identities only on the missing-file error path, distinguish cancellation from exhaustion, and honor newer navigation revisions. Tests cover removal/clear from direct and EOF-triggered error callbacks, plus removal/reordering of failed candidates. Follow-up read-only review reported no remaining Critical or Important issues.
- `QAbstractItemModelTester` probes GUI-role metatypes even in a QCoreApplication test; the Qt 6.12 tester's unregistered QPixmap probe produced an invalid-type warning. The headless test now links Qt Gui and registers QPixmap metadata without constructing a pixmap or GUI application. The warning is gone; no log suppression was added.
- Independent editing selection, scoped shortcuts, shared menu/context actions and InternalMove configuration have compile-only window assertions. No GUI/window/smoke tests ran, per the user's request. Actual Wayland pointer dragging, drop indicators, focus routing, bold-item presentation and GPU rendering remain unverified by this stage.

Subtitle display preferences stage (2026-10-22):

- `cmake --build --preset debug` succeeded, including NekoPlayer and both compile-only window suites. `ctest --test-dir build/debug -R '^(code-size|backend-startup|decoding|subtitles|playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 12/12 in 19.50 seconds. The subtitles suite also passed ten consecutive runs in 7.27 seconds.
- The sanitizer targets were rebuilt for this stage. After the final test synchronization change, `cmake --build --preset sanitizers --target subtitles_test` succeeded and `ctest --test-dir build/sanitizers -R '^(decoding|subtitles|playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 10/10 in 21.62 seconds without sanitizer errors.
- Real headless libmpv checks cover visibility without losing subtitle selection, scale/position/font delivery, rapid adjustments, normalization, nonfinite rejection, immediate save before event delivery, restart/idle restoration, stop/reopen and renderer recovery. Legacy, malformed and mixed configuration fields retain independent defaults. The missing visibility API failed before implementation.
- Two intermediate broad Debug runs passed 11/12 and exposed test synchronization errors. A synchronous backend `sid=1` query can precede the controller's queued selected-track snapshot; the test now waits for the corresponding selected track before capturing it. A font reset could initially match the default before the preceding font change was observed; the test now waits for that first transition before requesting the reset. These fixes add explicit state boundaries rather than sleeps or larger timeouts.
- Read-only review reported no Critical, Important or Minor findings. Formatting, `git diff --check` and the 500/800-line checks passed. The private controller header is 460 lines and the listed window-test exception is 561 lines; the subtitle-specific components remain separate, ordinary files.
- Draft editing, Apply/OK/Cancel/defaults, preservation of unavailable font names, shared menu actions and shortcuts have compile-only GUI assertions. No GUI/window/smoke tests ran, per the user's request. Actual subtitle pixels, font fallback, ASS layout, Wayland shortcut routing and GPU rendering remain unverified by this stage.

Hardware decoding preferences and mpv 0.41 navigation compatibility stage (2026-10-22):

- `cmake --build --preset debug` succeeded, including NekoPlayer and the compile-only window suite. `ctest --test-dir build/debug -R '^(code-size|backend-startup|decoding|playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 11/11 in 18.44 seconds.
- `cmake --build --preset sanitizers --target decoding_test playback_test playlist_test playlist_modes_test tracks_test session_test tuning_test navigation_test playback_time_test` succeeded. `ctest --test-dir build/sanitizers -R '^(decoding|playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 9/9 in 20.68 seconds without sanitizer errors. Formatting, `git diff --check` and 500/800-line checks passed; the private controller header is 459 lines, navigation implementation 91 lines and window tests 554 lines.
- The real headless decoding suite verifies runtime `hwdec=auto-safe/no` changes without reloading media or leaving pause, retained position/audio/external subtitle selection, idle software-preference restoration, rapid toggles followed by immediate save, restart, renderer recovery, stop/reopen, and missing/malformed/boolean/numeric configuration values. Missing-setter and ignored-software-configuration failures were observed before implementation. These tests check preference delivery and persistence with null video output; they do not exercise a physical hardware decoder or decoder-switch visual quality.
- The first broad runs failed navigation (Debug 10/11 and sanitizer 8/9), exposing a race in the previous chapter stage despite its earlier passing run. Diagnostic queries confirmed position 0 and an unavailable backend `chapter` property while paused. Version-specific mpv 0.41 source explains the stale chapter flag when an ordinary seek overwrites a queued native chapter seek; master already resets that state in a different location. The app now seeks the owned chapter timestamp directly rather than changing the system library or fabricating UI state. A six-transition regression failed before this fix and now verifies both actual backend chapter 0 and cleared application intent after an overriding seek.
- Read-only review identified a settlement gap for chapters sharing a timestamp. A four-chapter fixture reproduced actual chapter 2 with stale navigation target 1. The fix tracks SEEK after the latest command acknowledgment and confirms the final chapter asynchronously after PLAYBACK_RESTART; early queries only confirm matching targets. State mutations precede signals, preserving reentrant requests. The new shared-start regression passes, and the entire navigation suite passed five consecutive runs in 17.84 seconds before final broad verification. Follow-up review found no remaining Critical/Important issues.
- F5, menu/context-menu sharing, draft edits, Apply, Cancel and OK assertions compile only. No GUI/window/smoke tests ran, per the user's request. Real Wayland dialog interaction, active GPU hardware decoding, switching artifacts and hardware performance remain unverified.

Chapter navigation and time seeking stage:

- `cmake --build --preset debug` succeeded, including NekoPlayer and the compile-only window suite. `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 10/10 in 16.95 seconds.
- `cmake --build --preset sanitizers --target playback_test playlist_test playlist_modes_test tracks_test session_test tuning_test navigation_test playback_time_test` succeeded. `ctest --test-dir build/sanitizers -R '^(playback|playlist|playlist-modes|tracks|session|tuning|navigation|playback-time)$' --output-on-failure` passed 8/8 in 18.98 seconds without sanitizer errors. Formatting, `git diff --check` and code-size checks passed; the private controller header is 453 lines and window tests are 496 lines.
- Real headless libmpv tests cover UTF-8 chapter titles, missing titles, copied snapshot lifetime, stop/replacement/failure clearing, same-media reload with delayed Qt event draining, invalid chapter indices, and chapter selection without reload or leaving pause. The same-media reload check retained all three chapters; the suspected observer-coalescing loss was not reproduced. Time parsing tests cover seconds/minutes/hours, fractional milliseconds, long durations, malformed input, nonfinite/overflow rejection and formatting carry. Missing navigation methods and seven placeholder time-parser/formatter failures were observed before implementation.
- Review found rapid navigation used stale observed state: two immediate next-chapter calls reached chapter 1 instead of chapter 2. The regression failed before the fix and now passes. A pending navigation target accumulates requests; the latest command is confirmed with an asynchronous chapter query, including no-op selections. Ordinary seek, stop, replacement and reset cancel that target. Tests also cover direction reversal, backend changes after a no-op request, pending-seek cancellation and stop/reopen. A second review finding concerned disabled QAction boundaries before observations arrived; a dedicated navigation-intent signal now refreshes those boundaries immediately, while chapter checks retain the observed playback value. Follow-up review found no remaining Critical/Important issues.
- A 1.250-second request in a 25 fps fixture may display the frame at 1.280 seconds. Integration checks allow up to 0.08 seconds of settling/frame alignment while still verifying the chapter index; millisecond request precision does not imply every millisecond has a frame.
- Window assertions for chapter checks, rapid mixed QAction sequences, pause preservation and time-dialog validation compile only. No GUI/window/smoke tests ran, per the user's request. Actual Wayland shortcut routing, interactive dialogs and menu presentation remain unverified in this stage.

Playlist repeat modes stage:

- `cmake --build --preset debug` succeeded, including NekoPlayer and the compile-only window suite. `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|playlist-modes|tracks|session|tuning)$' --output-on-failure` passed 8/8 in 14.03 seconds.
- `cmake --build --preset sanitizers --target playback_test playlist_test playlist_modes_test tracks_test session_test tuning_test` succeeded. `ctest --test-dir build/sanitizers -R '^(playback|playlist|playlist-modes|tracks|session|tuning)$' --output-on-failure` passed 6/6 in 15.91 seconds without sanitizer errors. Formatting, `git diff --check` and the 500/800-line limits passed.
- Real headless libmpv tests cover single-file native looping without reload, manual navigation under repeat-one, repeat-all EOF and navigation wrapping, missing and invalid files, exhausted-queue append/retry, stop/clear/empty behavior, and switching modes without reloading or leaving pause. Appending during a single-item repeat-all loop restores automatic next-item playback. Native repeat-one retains the selected second audio track and an external subtitle without another `fileLoaded` event. Initial mode tests failed on the missing Qt property before implementation.
- Read-only review found two important boundaries, each reproduced before its fix. A valid PCM WAV header with zero samples emits `FILE_LOADED` before failing: clearing failure history there made a two-row repeat-all queue load repeatedly (four errors observed instead of two). History now resets only on successful EOF or explicit navigation/reset; the regression verifies exactly two failures and one queue completion. A controlled EOF signal delivered after switching to repeat-one previously advanced from row zero to row one; the fallback now reloads the current row, while normal native looping still avoids reloads. This controlled signal tests queue behavior at the asynchronous boundary; it does not measure the frequency of a real user EOF race.
- Follow-up read-only review confirmed both fixes and reported no remaining Critical/Important findings.
- The window suite's mode action, checked state, menu title, navigation availability and pause-preservation assertions compile only. No GUI/window/smoke tests ran, per the user's request; actual Wayland shortcut delivery and menu interaction remain unverified.

Playback tuning stage:

- `cmake --build --preset debug` succeeded, including NekoPlayer and the compile-only window suite. `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|tracks|session|tuning)$' --output-on-failure` passed 7/7 in 10.12 seconds.
- `cmake --build --preset sanitizers --target playback_test playlist_test tracks_test session_test tuning_test` succeeded. `ctest --test-dir build/sanitizers -R '^(playback|playlist|tracks|session|tuning)$' --output-on-failure` passed 5/5 in 11.50 seconds without sanitizer errors. Formatting, `git diff --check` and source-size checks passed.
- Real headless libmpv tests cover speed/audio/subtitle changes without reloading or leaving pause, rapid keyboard-step accumulation before replies, range clamping, nonfinite rejection, renderer-reload and stop/reopen retention, fresh-controller defaults, and subsequent adjustments based on backend changes. Missing-method failures were observed before implementing the tuning API.
- A regression exposed mpv 0.41's float storage for delays: requesting 0.2 can be observed as 0.2000000029802322, so exact double comparisons are invalid. A second regression reproduced a stale pending value after setting the same delay twice: external -0.3 followed by +0.05 incorrectly became +0.25. Both are resolved by confirming the latest setter with an asynchronous property query rather than relying on equality or a property-change event. The no-op regression failed before this fix and passed afterward.
- Read-only review identified the no-op confirmation hole; follow-up review found no remaining Critical/Important issues in latest-request handling, reentrancy, menu ownership or shared actions.
- GUI assertions for tuning actions, observed titles, preserved pause and subtitle-submenu rebuilding compile only. No GUI/window/smoke tests ran, per the user's request. Real Wayland shortcut delivery, custom-input dialogs, audible pitch correction and physical audio/subtitle synchronization remain unverified.

Session recovery stage:

- `cmake --build --preset debug` succeeded, including NekoPlayer and the compile-only window suite.
- `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|tracks|session)$' --output-on-failure` passed 6/6 in 8.79 seconds.
- `cmake --build --preset sanitizers --target playback_test playlist_test tracks_test session_test` succeeded. `ctest --test-dir build/sanitizers -R '^(playback|playlist|tracks|session)$' --output-on-failure` passed 4/4 in 9.90 seconds without sanitizer errors.
- Real temporary-file/QSettings tests verify Unicode/spaced and duplicate paths, missing-file retention, selected-row preservation, empty-queue replacement, scalar normalization, unavailable destination errors, and byte-for-byte preservation of future-version/malformed files. A malformed-file regression failed before the fresh-snapshot validation fix: a second QSettings instance reused parsed data without retaining the first instance's FormatError, allowing an otherwise rejected file to be rewritten.
- Headless libmpv integration verifies idle queue restoration, explicit playback of the restored selection, mute/volume restoration, immediate close before initial property observations, subsequent user audio changes, clear/restart, media-directory memory and files saved before renderer readiness. Missing-behavior failures were observed before implementing store, queue restoration and session manager.
- GUI integration assertions cover restoring an idle queue and panel state, delaying play until renderer readiness, normal geometry saved when closing fullscreen, startup file replacement and clearing pending files. These tests compile only. Native Wayland geometry restoration, fullscreen close, file-dialog directory use and interactive error dialogs have not been executed, per the user's GUI-test skip.
- Read-only review caught pending-file loss on clear/render failure; the clear action now removes pending files, while renderer failures retain accepted files for future startup. No remaining must-fix findings were reported after the fixes and snapshot-validation follow-up.

Audio/subtitle track stage:

- Final results: the full Debug build succeeded; the selected Debug checks passed 5/5 in 8.14 seconds, and the ASan/UBSan integration suites passed 3/3 in 9.32 seconds with no reported sanitizer errors. Formatting and `git diff --check` also passed.
- Full Debug build includes the player and `window_test` executable. The window tests remain compile-only at the user's request; no GUI/window CTest entry is run in this stage.
- `ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|tracks)$' --output-on-failure` covers code limits, backend startup and the three non-GUI integration suites.
- `cmake --build --preset sanitizers --target playback_test playlist_test tracks_test` and `ctest --test-dir build/sanitizers -R '^(playback|playlist|tracks)$' --output-on-failure` exercise the same controller paths under ASan/UBSan, including copying borrowed mpv node data. GUI sanitizer execution remains skipped.
- New FFmpeg fixtures provide two audio tracks, an embedded subtitle, and an external Matroska with two subtitle streams. Track tests verify owned metadata, typed track-ID validation, async audio/subtitle switching and disabling without media reload, UTF-8/spaced external paths, asynchronous load failure and clearing on stop/replacement.
- Recovery tests cover internal/external/disabled selection, repeated recovery without duplicated external streams, preservation of a selected second stream from a single external file, and a second reset while restoration is pending. The external-subtitle loss and overlapping-reset failures were reproduced before their fixes. An intermediate failure also confirmed that mpv can reset audio selection when reloading with a different external track layout; final selection is restored after external loading completes.
- GUI wiring assertions now check backend selections in addition to QAction checks. They compile but have not been executed, so actual menu presentation, file-dialog behavior and subtitle pixels remain unverified.

Per-file playback history stage:

- `cmake --build --preset debug` succeeded, including NekoPlayer and compile-only window targets. `ctest --preset debug -R 'playback-history-store|resume|session|code-size' --output-on-failure` passed 5/5 after integration.
- Headless tests cover versioned history persistence, path cleanup, duplicate replacement, 100-entry retention, invalid/corrupt file preservation, switching between files, stop behavior and restoration in a new session when the session bookmark is zero. The complete headless CTest collection passed 24/24 in Debug and 24/24 with ASan/UBSan; no sanitizer errors were reported. GUI/window tests were not run, per the user's request.
- This stage's history is isolated at `<session path>.history.ini`; it is saved on media switch, stop, normal session save and EOF cleanup, not on position observations. Window-specific Wayland behavior remains unverified.

Previous debug verification (playlist navigation and completion):

- `cmake --build --preset debug` succeeded.
- `ctest --test-dir build/debug -R '^(playback|playlist)$' --output-on-failure` passed both non-GUI integration tests.
- `cmake -DNEKO_SOURCE_DIR="$PWD" -P cmake/CheckCodeSize.cmake` confirmed ordinary files are within 500 lines and the explicitly listed special files are within 800 lines.
- The playback test verifies libmpv initialization, `hwdec=auto-safe`, local-file load/seek/pause/stop, mute/volume, EOF and recovery. The playlist test also verifies reverse missing-file skipping, invalid selection rejection, EOF with a missing tail, append after EOF, append while paused and stop/replay/pause through the queue controller. Four regressions were reproduced against the previous implementation before fixes were applied.
- GUI/window tests were deliberately not run in this verification round at the user's request. `window_test` now expects local multi-file drops to append, multiple command-line files to start a queue and paused appends to update navigation immediately. These cases compile but still require native-Wayland execution before they can be considered verified.
- X11/Xvfb remains available through the explicit `debug-x11` and `sanitizers-x11` presets for CI regression.
- C++ source/header files are each below their applicable project limit; a direct line-count scan covered tracked and untracked source files.

The available non-GUI checks are not evidence for actual framebuffer composition, physical GPU drivers, active hardware decoding, HDR, mixed-scale multi-monitor behavior, real audio output/quality, audio-video sync on hardware, or Windows packaging. `hwdec=auto-safe` requests a safe hardware-decode route; `hwdec-current` from a real playback session is the evidence of whether it was selected. See the scope and limitations in [README](../README.md).
