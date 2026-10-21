# NekoPlayer

以 **C++20 / Qt 6 + libmpv** 为基础，目标是做出接近 PotPlayer 的桌面播放体验：打开即播、完整快捷键与右键菜单、灵活的字幕/音轨选择、播放列表和可理解的设置。

当前为可播放的早期版本：Qt Widgets + QOpenGLWidget 嵌入 libmpv，提供深色紧凑控制区、菜单、右键菜单和本地播放列表。本地媒体可通过打开、添加、拖放或命令行进入队列；支持暂停、停止、时间定位、上一项/下一项、音量、静音、全屏、音轨切换、内嵌字幕选择、外挂字幕加载、倍速与音频／字幕延迟调整。队列、音量及窗口偏好可在正常退出后恢复；完整偏好设置界面与高级画质能力留待后续开发。

## 构建与检查

需要 CMake 3.25+、Ninja、支持 C++20 的编译器、Qt 6.5+ Core/Widgets/OpenGLWidgets、libmpv 开发库和 pkg-config。原生 Wayland 测试还需要 Qt Wayland 平台插件、可用的 Wayland 桌面会话及 Qt Test、FFmpeg。Xvfb、xvfb-run 和 Openbox 仅用于显式的 X11 CI 回归。仅构建播放器可传入 `-DBUILD_TESTING=OFF`。当前使用 Linux/pkg-config；Windows SDK 查找与打包尚未配置。

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/NekoPlayer
./build/debug/NekoPlayer '/path/电影 example.mkv'
./build/debug/NekoPlayer first.mkv second.mkv third.mkv
./build/debug/NekoPlayer --check-backend
./build/debug/NekoPlayer --no-config
```

仅运行无需桌面会话的后端检查（当前按用户要求跳过 GUI 测试）：

```bash
ctest --test-dir build/debug -R '^(code-size|backend-startup|playback|playlist|tracks|session|tuning)$' --output-on-failure
```

内存检查预设，以下命令仅构建和执行非 GUI 集成测试：

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers --target playback_test playlist_test tracks_test session_test tuning_test
ctest --test-dir build/sanitizers -R '^(playback|playlist|tracks|session|tuning)$' --output-on-failure
```

在没有 Wayland 桌面会话的 CI 上，显式选择 X11 回归后端：

```bash
cmake --preset debug-x11
cmake --build --preset debug-x11
ctest --preset debug-x11
```

后端检查禁用用户 mpv 配置和脚本，使用空音视频输出，只验证创建、初始化、版本查询和销毁。环境探测到 Qt 6.12.0、mpv 0.41.0；pkg-config 的 mpv 2.5.0 是 client API 版本。

为规避 mpv 0.41.0 GPU 缩放 LUT 未初始化 padding 可能导致的 NaN，本应用显式将 `scale`、`cscale`、`dscale` 设为 `spline64`。该设置保留高质量重建滤镜，并覆盖放大、色度和缩小路径；mpv 上游已在后续版本修复 LUT padding。此兼容选项仍会覆盖用户缩放滤镜偏好，后续应基于运行时版本或实测决定是否移除。

CTest 自动生成包含音轨的彩色测试视频及无音轨版本，覆盖控制器、中文/空格路径、失败恢复、实际帧内容、菜单动作、定位预览、全屏恢复、隐藏/显示、GL 上下文重建和图形初始化失败。默认 `debug` 与 `sanitizers` 预设在原生 Wayland 运行窗口测试；`*-x11` 预设通过 Xvfb + Openbox 提供 CI 回归。Qt 的 offscreen 平台不能代替视频渲染检查。

测试使用 generic 主题和 compose 输入上下文隔离宿主 GTK/Fcitx 插件，音频使用 null 输出。正常启动仍使用桌面配置和真实音频输出；ASan/UBSan 与泄漏检查保持开启。`--smoke-test` 检查初始化和可选文件加载后自动退出。

## 常用操作

| 操作 | 快捷键 |
| --- | --- |
| 打开文件 | Ctrl+O |
| 添加文件 / 显示播放列表 | Ctrl+Shift+O / Ctrl+L |
| 加载外挂字幕 | Ctrl+Shift+S |
| 播放 / 暂停 | 空格 |
| 上一项 / 下一项 | 多媒体上一曲 / 下一曲键 |
| 后退 / 前进 5 秒 | ← / → |
| 音量减 / 加 | ↓ / ↑ |
| 静音 / 停止 | M / S |
| 全屏 / 退出全屏 | F 或双击 / Esc |
| 减慢 / 加快 0.1 倍 / 正常速度 | `[` / `]` / Backspace |
| 字幕提前 / 延后 100 毫秒 / 归零 | `,` / `.` / `/` |
| 音频提前 / 延后 50 毫秒 / 归零 | Shift+`,` / Shift+`.` / Shift+`/` |

打开文件会替换现有队列；添加文件、拖入一个或多个本地文件、或传入多个命令行位置参数会追加或创建队列。网络地址拖放会被拒绝并显示原因。全屏默认隐藏菜单与控制区，移动鼠标显示控制区，闲置后收起；错误会显示可见提示。正常播放禁用用户 mpv 配置、脚本、OSC 和默认键绑定，由 NekoPlayer 统一管理交互。

播放结束后自动进入下一项并跳过缺失文件；上一项按逆序跳过缺失文件。队列播完后追加媒体会启动新增项，暂停期间追加则保留当前播放。停止或播完后按空格可重新播放当前项。

正常退出会保存播放列表、当前行、音量／静音、窗口大小和最大化状态、列表面板显示状态及媒体对话框目录。再次启动只恢复队列，按空格才开始播放；命令行文件替换恢复队列。退出全屏时或在全屏下关闭，均保留之前的普通窗口状态，不在下次启动自动全屏。配置使用版本化 INI，Linux 默认位于 `$XDG_CONFIG_HOME/NekoPlayer/NekoPlayer/session.ini`（未设置时位于 `~/.config/` 下）；保存失败会提示，损坏或未知版本的配置不会被覆盖。`--no-config` 与 `--smoke-test` 均不读写配置。此阶段不保存播放进度、轨道和外挂字幕，也不提供崩溃恢复；同时运行多个实例时以最后保存的会话为准。

“声音 → 音轨”和“字幕”菜单可查看轨道编号、标题、语言、编码及当前选择，也可关闭音轨或字幕；右键菜单共用这些入口。加载外挂字幕后自动选中该轨道。切换轨道无需重新打开媒体，保持暂停状态；停止或换片时清空旧轨道。图形上下文重建时恢复音轨、字幕选择和已加载的外挂字幕。外挂字幕目前通过文件对话框加载，字幕拖放和字体设置尚未实现。

播放速度菜单提供 0.25–4 倍预设和自定义输入，并启用 mpv 音调修正。音频延迟范围为 ±10 秒，字幕延迟范围为 ±600 秒；正值延后、负值提前。菜单显示后端确认的值，快速连续按键累积调整。倍速和延迟在本次运行中跨停止、换片及渲染恢复保留，重新启动恢复为 1 倍和零延迟，不写入会话配置。

## 工程约束与验证边界

普通手写代码文件不超过 **500 行**。少数明确列入 `cmake/CheckCodeSize.cmake` 的窗口协调或桌面集成文件可不超过 **800 行**，新增例外必须附带理由；CTest 的 `code-size` 检查自动执行这两个限制。播放业务、GPU 资源、窗口协调与控制条分别实现，详见 [架构说明](docs/architecture.md)。使用 `clang-format`，警告默认开启；回调寿命变更必须运行 sanitizer 预设。

自动化以 Linux 原生 Wayland 为基准，并保留 Linux X11 软件 OpenGL 回归。当前实际验证为单个 1.75 缩放显示器；真实扬声器听感、音画同步、混合缩放多显示器、不同 GPU、硬解、HDR 与 Windows 尚需专项验收；不能由 null 音频或软件渲染测试推断。合成器可以使未聚焦的测试窗口 suspended，因此自动化通过共享 QAction 验证命令行为；真实 Wayland 键盘焦点/快捷键路由需要桌面手工验收。GL 上下文重建时重新打开当前本地文件并恢复时间和暂停状态，可能短暂中断播放。当前验证轮次按要求跳过 GUI 集成测试，详细证据见 [验证记录](docs/verification.md)。

参考源码见 [docs/references.md](docs/references.md)，skills 见 [docs/skills.md](docs/skills.md)。`references/` 已忽略，不参与主项目构建或 Git 版本管理。项目自身许可证尚未选定。
