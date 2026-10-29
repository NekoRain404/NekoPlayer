# 开发指南

## 环境与依赖

- CMake 3.25+、Ninja、支持 C++20 的 GCC/Clang。
- Qt 6.5+：Core、Concurrent、Widgets、OpenGLWidgets、Network（测试）、LinguistTools。
- libmpv 开发库、FFmpeg 开发库（libavformat/libavcodec/libavutil/libswscale）、pkg-config。
- 测试额外需要 Qt Test、Qt Network（仅本地 HTTP 测试服务）与本地 `ffmpeg` 可执行文件（生成测试媒体）。缩略图预览直接使用 FFmpeg 解码，因此即使 `-DBUILD_TESTING=OFF` 仍需要 FFmpeg 开发包。
- 原生 Wayland 测试需要 Qt Wayland 平台插件和可用的 Wayland 会话；Xvfb、xvfb-run、Openbox 仅用于显式 X11 回归。

Arch 依赖与打包说明见 [packaging.md](packaging.md)。

## 构建与测试

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/NekoPlayer
./build/debug/NekoPlayer --check-backend
```

- CTest 会为每个测试注入 `LC_ALL=C`/`LANG=C`（libmpv 要求 C 数值 locale），无需手动设置。
- 窗口测试已注册，只在桌面预设中运行：`debug`/`sanitizers` 在原生 Wayland，`*-x11` 通过 Xvfb + Openbox。
- GitHub Actions 的 `arch-smoke` 运行在 `archlinux:base-devel` 容器中，与打包目标保持一致：完整构建 + 核心 smoke（code-size、后端启动、媒体源/M3U8/库/扫描/缩略图/纯逻辑测试）。托管容器没有 GPU、音频与显示，libmpv 的视频输出相关用例（帧步进、媒体信息刷新）在容器内不稳定，因此完整 89 项套件与窗口集成仍以桌面预设（`debug`/`debug-x11`）为验收门槛。CI 设置 `QT_QPA_PLATFORM=offscreen`、可写 HOME/XDG 目录、字体缓存与 `ctest --timeout 180`，并对同一 ref 的新推送启用并发取消。
- `always_on_top_window_test`、`playlist_maintenance_window_test` 仅编译、不注册。
- 生命周期、回调和关闭顺序变更后运行 sanitizer 预设的聚焦测试：

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers
ASAN_OPTIONS=detect_leaks=0 ctest --preset sanitizers -R '<相关测试>'
```

`--smoke-test` 只验证初始化与可选文件加载，不能代替真实视频渲染、硬解或 HDR 验收。

## 目录结构

```
src/
  playback/
    mpv/        libmpv 控制器、事件、属性观察、轨道/章节/音频设备
    media/      媒体源、轨道/章节值类型、时间与画面参数
    playlist/   队列模型/控制器、随机顺序、自然排序
    render/     QOpenGLWidget 视频表面
  session/      会话、播放历史、轨道偏好、配置文件权限
  input/        拖放、媒体扩展名、文件夹后台扫描
  preview/      FFmpeg 缩略图解码与请求合并
  playlist/     M3U8 读写（含 #EXTINF 元数据）
  library/      媒体库文件夹/条目持久化与扫描协调
  ui/
    window/     主窗口、全屏/小窗、窗口动作、标题与图标
    dialogs/    偏好设置、媒体库、快捷键、字幕样式、媒体信息
    menus/      共享 QAction 菜单（文件/播放/视频/字幕/音轨）
    panels/     播放区、播放列表、控制条、缩略图预览
    theme/      外观 token、QSS、运行时图标、语言管理
  cli/          命令行参数辅助
tests/
  playback/ session/ playlist/ preview/ input/ library/ cli/ ui/ window/
               与 src 分层对应的测试
  cmake/       测试注册模块与 FFmpeg 媒体夹具
  data/        字幕、章节等固定测试数据
data/          desktop、metainfo、图标与 QSS 资源
docs/          文档索引见 docs/README.md
packaging/     Arch 打包脚本与说明（生成物不入库）
references/    忽略的上游学习克隆（不参与构建）
```

分层原则：播放状态/命令、图形资源、桌面交互分离；`ui/` 不直接发原始 mpv 命令，优先复用共享 `QAction`。详见 [architecture.md](architecture.md)。

## 运行时注意事项

- libmpv 要求 C 数值 locale；应用启动时设置 `setlocale`，CTest 为每个测试注入 `LC_ALL=C`/`LANG=C`。
- 为规避 mpv 0.41.0 GPU 缩放 LUT 未初始化 padding 可能导致的 NaN，应用显式把 `scale`、`cscale`、`dscale` 设为 `spline64`。上游已修复该 padding；此兼容设置目前仍会覆盖用户的缩放滤镜偏好，后续应根据运行时版本或实测决定是否移除。
- 正常播放禁用用户 mpv 配置、脚本、OSC 与默认键绑定，并设置 `ytdl=no`；网页地址不会进入外部解析器。
- `--fullscreen` 在窗口显示后进入全屏；`--start-paused` 只在提供命令行媒体时对首个成功加载的文件暂停一次。两者都不写回持久状态。
- 同一配置目录只运行一个实例；再次启动会把位置参数转交给已运行窗口。`--no-config`、`--smoke-test`、`--check-backend` 不参与单实例。
- `--smoke-test` 只验证初始化和可选文件加载；offscreen 或空音频输出不能证明真实视频渲染、硬件解码或 HDR。

## 代码约定

- 普通手写文件目标 ≤500 行，自动检查容忍至 550；`cmake/CheckCodeSize.cmake` 明确列出的窗口协调文件可到 800，新增例外需写明理由。
- 使用 `clang-format`；构建默认开启 `-Wall -Wextra -Wpedantic`，应保持 0 警告。
- 显式错误处理、RAII、Qt 父子所有权；GUI 工作留在 GUI 线程，后台任务通过 QtConcurrent/QThreadPool 并在 GUI 线程提交结果。
- 持久化文件使用原子写、schema 版本、大小/条数上限，并在可能包含敏感 URL 时限制为 0600。
- 测试遵循 Arrange-Act-Assert；新增行为优先补 headless 测试，窗口行为补注册的窗口测试。

## 发布与打包

版本号来自 `CMakeLists.txt` 的 `PROJECT_VERSION`；About 文本与 `--version` 使用运行时值。当前开发版本为 **0.2.0**，以 `v0.2.0-rc1` 这类预发布标签发布。

发布流程：

1. 更新 `CHANGELOG.md` 与 `PROJECT_VERSION`，确认 `ctest --preset debug`（以及可选的 `debug-x11`）全绿。
2. 提交后打标签并推送：`git tag -a v0.2.0-rc1 -m 'NekoPlayer 0.2.0-rc1' && git push origin v0.2.0-rc1`。
3. `.github/workflows/release.yml` 会生成 `nekoplayer-<version>.tar.gz`、`SHA256SUMS` 并创建 GitHub 预发布；标签含 `rc`/`beta`/`alpha` 时自动标记为 pre-release。
4. 发布后更新 `packaging/arch/PKGBUILD` 的 `pkgver`、源码 URL 与 `sha256sums`（用 `updpkgsums` 或校验发布附件）。PKGBUILD 始终指向**最近一次已发布**的源码归档，不在发布前预填未知 checksum。

安装布局、桌面数据库刷新与 Arch 打包、checksum 流程见 [packaging.md](packaging.md)；每次发布的构建/测试证据记录到 [verification.md](verification.md)。

## 文档维护

文档索引与归档规则见 [docs/README.md](README.md)。审计、交接等一次性材料放入 `docs/audit/` 或 `docs/history/` 并标注日期。
