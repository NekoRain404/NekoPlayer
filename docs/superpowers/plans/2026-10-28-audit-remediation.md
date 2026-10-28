# 审计整改实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `docs/audit-2026-10-28.md` 中的 P0 问题与高价值 P1 问题，并补齐回归测试。

**Architecture:** 保持现有分层（playback/session/input/preview/ui）。正确性修复优先在控制器与存储边界内完成，
不改变公共 UI 结构；体验与文档修复独立进行。

**Tech Stack:** C++20、Qt 6 Widgets、libmpv render API、FFmpeg、CTest。

**Spec:** `docs/audit-2026-10-28.md`

> **执行状态（2026-10-28）:** Task 1–9 已实施。验证结果：非窗口套件 73/73（66.41 秒，
> 自动注入 `LC_ALL=C`）；窗口套件 11/13，剩余 5 个子用例为既有的小窗/全屏切换问题；
> 英文目录 `lrelease` 436 条、0 未完成；`code-size` 通过。未纳入项见文末。

## Global Constraints

- 单文件不超过 500 行（自动检查允许至 550；`mainwindow.cpp`、`window_test.cpp` 例外 800）。
- 不引入新依赖；GUI 工作保持在 GUI 线程。
- 所有测试命令必须带 `LC_ALL=C`（本次修复后由 CTest 环境变量统一提供）。
- 提交信息使用 `fix:` / `test:` / `docs:` 前缀；本计划不自动 commit，由用户决定。
- 修改回调/生命周期代码后运行 sanitizer 预设的聚焦测试。

## Review Focus

- 非 C locale 下启动与测试：任何直接构造 `mpv_handle` 的测试不得崩溃。
- Stop 后事件乱序：EOF/FILE_LOADED 不得改变已停止状态。
- 媒体离线/被替换：轨道记忆保留与丢弃必须可区分。
- 损坏的附属配置文件：不得阻止会话保存或永久禁用历史。
- 键盘焦点：Tab/方向键/空格在列表与按钮上保持标准行为。

---

### Task 1: 测试 locale 与空句柄防护

**Files:**
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/decoding_test.cpp:13-18`
- Modify: `tests/subtitles_test.cpp`, `tests/video_color_test.cpp`, `tests/playback_test.cpp`, `tests/tuning_test.cpp`（同类辅助函数）
- Modify: `README.md` 构建与检查命令

**Interfaces:**
- Produces: CTest 环境统一包含 `LC_ALL=C`、`LANG=C`。

- [ ] Step 1: 在 `tests/CMakeLists.txt` 末尾为当前目录所有已注册测试追加环境变量。
- [ ] Step 2: 运行 `ctest --preset debug -R decoding --output-on-failure`（默认 locale）验证通过。
- [ ] Step 3: 测试辅助函数在 `player.handle() == nullptr` 时返回空字符串，避免 SEGV。
- [ ] Step 4: 运行 `ctest --preset debug -E window`，期望 71/71。
- [ ] Step 5: README 的 `ctest --preset debug` 说明改为无需手动设置 LC_ALL。

### Task 2: Stop/EOF 事件归属

**Files:**
- Modify: `src/playback/mpvcontroller_p.h`（墓碑集合）
- Modify: `src/playback/mpvcontroller.cpp:308-339`（stop 登记墓碑、重置 playingEntry）
- Modify: `src/playback/mpvcontroller_events.cpp:207-269`（END_FILE/FILE_LOADED 拒绝墓碑）
- Modify: `src/playback/playlistcontroller.cpp:31-35`（stopRequested 清 playbackQueued_）
- Test: `tests/mpv_open_failure_test.cpp` 或新增 `tests/stop_interleaving_test.cpp`

**Interfaces:**
- Produces: `Impl::invalidatedEntries`（`QSet<int64_t>`，容量上限 64）。

- [ ] Step 1: 写失败测试：构造 headless 控制器，模拟 Stop 后到达的 END_FILE(EOF) 与 FILE_LOADED。
- [ ] Step 2: 运行测试确认失败（当前会 emit playbackFinished/fileLoaded）。
- [ ] Step 3: 实现墓碑与 `playingEntry = -1`，`handleFileLoaded` 拒绝未知 entry。
- [ ] Step 4: `PlaylistController` 在 stopRequested 时清 `playbackQueued_`。
- [ ] Step 5: 运行 `ctest -R 'mpv-open-failure|playlist|stop'` 通过。

### Task 3: 轨道偏好保留离线媒体

**Files:**
- Modify: `src/session/trackpreferencestore.{h,cpp}`
- Modify: `src/session/sessionmanager.cpp:355-363`
- Test: `tests/track_preference_store_test.cpp`

**Interfaces:**
- Produces: `TrackPreferenceStore::entryState(entry)` 返回 `Missing | Current | Replaced | Invalid`。

- [ ] Step 1: 写失败测试：保存偏好后移除媒体文件，load+save 后条目仍在；改变 size 后条目被丢弃。
- [ ] Step 2: 实现三态判定；`normalizedEntries`/`save` 仅丢弃 Replaced/Invalid。
- [ ] Step 3: `SessionManager::saveTrackPreferences` 使用同一判定。
- [ ] Step 4: 运行 `ctest -R 'track-preference'` 通过。

### Task 4: 附属存储错误不再锁存

**Files:**
- Modify: `src/session/playbackhistorystore.cpp`（仅 schema 错误进入 error）
- Modify: `src/session/sessionmanager.cpp:98,157-166,211-218`（分离错误）
- Modify: `src/ui/windowsession.cpp:55-71`（仅会话错误弹窗）
- Test: `tests/playback_history_store_test.cpp`, `tests/session_test.cpp`

- [ ] Step 1: 写失败测试：历史文件含一条无效记录时，`save()` 仍能写会话且无错误。
- [ ] Step 2: 实现错误分类与重写；坏条目在 load 时丢弃并允许后续保存。
- [ ] Step 3: 运行 `ctest -R 'playback-history|session'` 通过。

### Task 5: 键盘可访问性

**Files:**
- Modify: `src/ui/windowactions.cpp:77,129-130,258-259`
- Modify: `src/ui/transportbar.cpp:211-228`
- Modify: `src/ui/playlistpane.cpp:77-94,118-122`
- Test: `tests/window_test.cpp` 或新增 `tests/keyboard_navigation_test.cpp`

- [ ] Step 1: Tab 快捷键改为 Ctrl+Tab（同步 README/测试断言）。
- [ ] Step 2: 为视频区/控制条相关动作设置 `WidgetWithChildrenShortcut`。
- [ ] Step 3: 给播放列表视图与可聚焦按钮安装 ShortcutOverride 过滤器（↑/↓/空格）。
- [ ] Step 4: 窗口测试：列表聚焦时按 ↓ 选择移动且音量不变；按钮聚焦时空格激活按钮。

### Task 6: 缩略图管线加固与提速

**Files:**
- Modify: `src/preview/ffmpegthumbnaildecoder.cpp:17-31,87-101,130-226`
- Modify: `src/preview/thumbnailservice.cpp:34-117`
- Modify: `src/ui/mainwindow.cpp:490-530`（防抖）
- Test: `tests/thumbnail_decoder_test.cpp`, `tests/thumbnail_service_test.cpp`

- [ ] Step 1: 写失败测试：`file://` 之外的协议被拒绝；超时请求在 5s 内返回错误。
- [ ] Step 2: 添加 AVDictionary：`protocol_whitelist=file`、`rw_timeout`、`probesize`、`analyzeduration`；
      解码器 `max_pixels`、`thread_count=1`；interrupt 增加墙钟 deadline。
- [ ] Step 3: 解码只保留最后候选帧，结束时转换一次。
- [ ] Step 4: `ThumbnailService::request` 量化秒数；MainWindow 用 120ms 单次定时器防抖。
- [ ] Step 5: 运行 `ctest -R 'thumbnail'` 通过。

### Task 7: 补齐英文翻译

**Files:**
- Modify: `translations/NekoPlayer_en.ts`
- Test: `tests/language_preferences_test.cpp`（可加计数断言）

- [ ] Step 1: 运行 `lupdate -no-obsolete src -ts translations/NekoPlayer_en.ts`。
- [ ] Step 2: 翻译全部 unfinished 条目，保持术语与现有译文一致。
- [ ] Step 3: 运行 `lrelease`，确认 0 unfinished。
- [ ] Step 4: 更新 README 的翻译声明。

### Task 8: 文档与打包一致性

**Files:**
- Modify: `README.md`、`docs/verification.md`、`docs/packaging.md`、`data/nekoplayer.metainfo.xml`

- [ ] Step 1: 修正测试数量、窗口测试策略、FFmpeg/LinguistTools 依赖说明。
- [ ] Step 2: 修正打包 checksum 流程（与 PKGBUILD 一致）。
- [ ] Step 3: 修正 metainfo homepage 与 README 缺失快捷键。

### Task 9: 高价值 P1 行为修复

**Files:**
- Modify: `src/playback/mpvcontroller.cpp:343-349`（duration=0 不钳上界）
- Modify: `src/playback/mpvcontroller_events.cpp:152-166`（取消命令回复不 report）
- Modify: `src/ui/screenshotmenu.cpp:36-40`（失败反馈）
- Modify: `src/ui/mainwindow.cpp:474-488`（小窗/全屏错误与反馈可见）
- Modify: `src/ui/preferencesdialog.cpp:261-264`（取消回滚 glass 属性）
- Modify: `src/ui/mainwindow.cpp:449-465`（空闲时禁用后退/前进）
- Modify: `src/playlist/m3u8store.cpp:94-97`（允许 currentIndex=-1 保存）
- Test: 对应现有测试文件

- [ ] Step 1-7: 每项先写失败测试再实现，逐项运行对应测试。
- [ ] Step 8: 运行非窗口套件全绿。

## 剩余工作（未纳入本轮）

- P0-5 的完整重构：恢复加载登记为受跟踪 open、`VO_INIT_FAILED` 后重试与错误上报。
- P1 性能批次：批量删除合批、`stateChanged` 合并、持久化异步化、排序移出 GUI 线程。
- P1 安全批次剩余：URL 凭据默认脱敏、外部 `m3u8` 自动播放确认。
- M3U8 HLS 检测与 `#EXTINF`/`#EXT-X-*` 保真；“清空最近”与续播历史统一。
- 仍未实现的功能：字幕搜索/下载、媒体库索引、快捷键自定义、M3U8 `#EXTINF` 元数据保真。

## Self-Review

- 每个 P0 均有对应任务与回归测试；P1 仅纳入无需真机验证的项。
- 任务 2/5 涉及窗口测试，需在真实 Wayland 会话运行；其余使用 headless。
- 未纳入：P0-5 渲染恢复重构（风险高，建议单独分支并在真机验证）、功能缺口清单。

