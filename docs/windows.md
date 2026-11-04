# Windows 兼容方案

> 状态：第一阶段已落地（2026-11-04）：可选 DBus、跨平台单实例、Windows 资源与预设、窗口外观回退、便携打包脚本与 Windows CI 任务。Windows 构建/运行仍以 CI 与真机验收为准；本机没有 Windows 工具链。

## 结论

Windows 兼容可行，工作量集中在**构建依赖、桌面集成、窗口材质、打包发布**四块，播放核心（libmpv + OpenGL render API、FFmpeg 缩略图）本身是跨平台的。

推荐路线：

1. **第一目标：MSYS2 UCRT64 + Ninja**。它与当前 `pkg-config` 依赖方式、GPL 依赖组合和 Arch 构建习惯最接近，能最快得到一个可运行版本。
2. **第二目标（可选）：MSVC + vcpkg**。需要补 `Findmpv`/`FindFFmpeg` 或不依赖 pkg-config 的查找逻辑；建议等 MinGW 版本稳定后再做。
3. **发布形态先做便携 zip**（exe + Qt/libmpv/FFmpeg DLL），稳定后再加 Inno Setup 安装器、文件关联与代码签名。

## 当前进度（2026-11-04）

- **P0 已落地**：Qt DBus 可选、MPRIS 条件编译、`windows`/`windows-cross` 预设、MinGW 工具链文件、Windows `.rc`/`.ico`/manifest、按平台安装规则、`windows-msys2`（UCRT64）CI 与 Windows 便携 zip 发布任务。CI 首次运行发现 MSYS2 已弃用 MINGW64 且其仓库不再提供 mpv，已切换到 UCRT64；构建结果以最新 CI 为准。
- **P1 已落地**：单实例改为 `QLocalServer`/`QLocalSocket`，Linux 行为与端点命名保持，`single-instance` 测试通过；桌面媒体控制抽象与 Windows 全局媒体键尚未实现。
- **P2 第一步已落地**：Windows 顶层窗口改为不透明回退，DWM 暗色标题栏按应用调色板设置（属性 20，回退 19）。Mica/Acrylic、运行时主题切换、对话框透明属性统一仍待完成。
- **P4 骨架已落地**：`packaging/windows/deploy.sh`、manifest/图标/版本资源、release 工作流 Windows job；安装器、文件关联与代码签名未做。
- **仍缺**：Windows 真机运行与视觉验收、SMTC 或全局媒体键、双显示器/HiDPI/全屏验收、D3D11/HDR 路径。
- Linux 回归：Debug `-j4` 构建无警告，串行全量 **102/102** 通过（见 [verification.md](verification.md)）。

## 现状审计

| 区域 | 现状 | Windows 影响 | 处理 |
| --- | --- | --- | --- |
| CMake Qt 组件 | `CMakeLists.txt:11` 强制 `DBus`，`CMakeLists.txt:111` 链接 `Qt6::DBus` | Windows 版 Qt 不提供 DBus 模块，**配置阶段直接失败** | 改为 `QUIET` 可选组件，按平台/可用性编译 MPRIS |
| MPRIS | `src/desktop/mprisplayer.cpp` 全部基于 QDBus；`windowdesktop.cpp:53` 无条件创建 | 无法编译 | 用 `NEKOPLAYER_HAS_MPRIS` 条件编译 + 平台媒体控制抽象 |
| 单实例 | `src/main.cpp:27-35`、`60-260`、`302-341` 使用 Unix domain socket、`poll`/`fcntl` | Windows 无此实现，功能缺失 | 改用 Qt Network 的 `QLocalServer`/`QLocalSocket`（两平台通用），删除约 200 行 Unix 专用代码 |
| 托盘 | `QSystemTrayIcon`（`windowdesktop.cpp`） | 可用 | 无需改动，做视觉验收 |
| 媒体键 | `QKeySequence(Qt::Key_MediaPlay/...)` 应用级快捷键 | 仅窗口聚焦时生效；Windows 用户预期全局媒体键/系统媒体浮层 | 阶段 3 用 `RegisterHotKey`，再评估 SMTC（WinRT） |
| 渲染 | `QOpenGLWidget` + `MPV_RENDER_API_TYPE_OPENGL`（`videowidget.cpp:69-74`） | 理论可移植；依赖显卡 OpenGL 驱动，ANGLE/GLES 组合需实测 | 在 Windows 实机验证；失败再评估 D3D11 render API（同时是原生 HDR 的前置条件） |
| 窗口材质 | 顶层 `WA_TranslucentBackground`（`mainwindow.cpp:75`）、Wayland 合成器模糊说明 | Windows 顶层透明 + 原生边框可能变黑、无阴影或点击穿透；玻璃说明文字不适用 | 增加平台外观策略：Windows 默认不透明回退，Win11 可选 Mica/Acrylic；暗色标题栏 |
| 窗口测试 | `tests/cmake/window_tests.cmake:1-23` 默认 `wayland`，x11 依赖 `xvfb-run`/`openbox` | Windows 上配置直接失败 | 增加 `windows`/`offscreen` 后端分支，Windows 默认不启用 Wayland/X11 |
| MPRIS 测试 | `tests/cmake/session_tests.cmake:98` 无条件注册 `mpris_test` | 编译/运行失败 | 用同一 `NEKOPLAYER_HAS_MPRIS` 条件保护 |
| 安装规则 | `CMakeLists.txt:119-127` 安装 `.desktop`、hicolor 图标、metainfo | Windows 无意义 | 用 `if(UNIX)` 包裹，Windows 走部署脚本 |
| 路径/UNC | `mediasource.cpp` 拒绝带 host 的 `file://` URL | Windows 网络共享（`\\server\share`）拖放会被拒绝 | Windows 下接受 UNC 并规范化为本地路径 |
| 路径大小写 | 历史/去重以字符串比较本地路径 | `C:\A.mkv` 与 `c:\a.mkv` 可能被当成不同媒体 | 身份比较使用 `canonicalFilePath()`，或在 Windows 做大小写不敏感比较 |
| 播放列表 | `m3u8store.cpp:177` 原样写入绝对路径 | Windows 反斜杠在其他播放器/M3U8 消费端可能被当转义 | 保存本地路径时统一转 `/`（`QDir::fromNativeSeparators`） |
| 可执行文件图标/清单 | 无 `.ico`、`.rc`、manifest | 资源管理器/任务栏显示默认图标；无版本信息 | 增加 `packaging/windows/nekoplayer.rc`、`.ico`、DPI/兼容性 manifest |
| CI/发布 | `.github/workflows/ci.yml` 仅 Arch 容器；`release.yml` 仅源码包 | 没有 Windows 构建证据与二进制 | 增加 MSYS2 job 与 tag 发布矩阵 |
| HDR/硬解 | tone mapping 走 mpv 通用选项；硬解 `auto-safe` | Windows 会选 d3d11va/dxva2，可工作；**原生 HDR 输出**当前 GL 路径不承诺 | 作为边界写明；原生 HDR 需要 D3D11 渲染上下文与显示 HDR 状态管理 |

## 分阶段实施

### P0 — 构建可配置（最先做，约 0.5–1 天）

目标：Linux 行为不变，Windows 能 configure/build 出主体；MPRIS 与 Unix 单实例暂时禁用。

- `CMakeLists.txt`
  - `find_package(Qt6 6.5 REQUIRED COMPONENTS Core Concurrent Widgets OpenGLWidgets LinguistTools Network)`
  - `find_package(Qt6 6.5 QUIET COMPONENTS DBus)`；仅当 `UNIX AND Qt6DBus_FOUND` 时加入 `desktop/mprisplayer.cpp`、链接 `Qt6::DBus` 并定义 `NEKOPLAYER_HAS_MPRIS`。
  - 安装规则用 `if(UNIX)` 包裹；Windows 增加 `install(TARGETS NekoPlayer RUNTIME DESTINATION .)`。
  - 新增 `windows` configure/build/test preset（Ninja，MSYS2 环境）。
- `src/ui/window/windowdesktop.cpp`：MPRIS 创建放入 `#ifdef NEKOPLAYER_HAS_MPRIS`；后续替换为平台媒体控制工厂。
- `src/main.cpp`：Unix 单实例代码先整体 `#ifdef Q_OS_UNIX` 保住 Linux，Windows 走 P1 的新实现。
- `tests/cmake/window_tests.cmake`：`WIN32` 时默认 `offscreen`（或 `windows`），不设置 Wayland/X11 环境，不查找 `xvfb-run`。
- `tests/cmake/session_tests.cmake`：`mpris_test` 仅在 `NEKOPLAYER_HAS_MPRIS` 时注册。
- CI：新增 `windows-msys2` job（`msys2/setup-msys2`，MINGW64 工具链 + qt6-base/qt6-tools/mpv/ffmpeg/pkgconf/cmake/ninja），构建 `-j4` 并跑与 Linux 相同的确定性 smoke 集合。

### P1 — 单实例与桌面控制抽象（约 1–2 天）

- 新增 `src/desktop/singleinstance.{h,cpp}`：
  - `QLocalServer`/`QLocalSocket` 实现现有的“单行 JSON 参数转发”协议；
  - 两平台统一使用 `AppConfigLocation` 哈希命名，Windows 为命名管道，Unix 为本地 socket；
  - 处理残留端点：先连接探测，失败后 `QLocalServer::removeServer()` 再监听。
- 新增 `src/desktop/mediacontrols.h`：`MediaControls` 接口（播放状态、元数据、Play/Pause/Next/Previous/Seek/OpenUri、Raise/Quit 信号），Linux 由 `MprisMediaControls` 适配现有 `MprisPlayer`；Windows 先空实现 + 媒体键，阶段 3 接 SMTC。
- `main.cpp` 的 Unix 专用 socket 代码删除，减少平台分叉。
- 单元测试：`tests/desktop/single_instance_test.cpp`（启动 server、转发参数、残留端点恢复），Linux 即可验证跨平台 Qt 实现。

### P2 — 窗口与外观（约 2–4 天，含实机验收）

- 新增平台外观策略（建议 `src/desktop/windowbackdrop.{h,cpp}`）：
  - Windows：`DwmSetWindowAttribute(DWMWA_USE_IMMERSIVE_DARK_MODE)` 跟随主题；Win11 22H2+ 可选 `DWMWA_SYSTEMBACKDROP_TYPE`（Mica/Acrylic）；不可用时保持不透明背景。
  - Linux：保持现有 Wayland 透明 + 合成器模糊。
- 评估并消除顶层 `WA_TranslucentBackground` 在 Windows 黑底/点击穿透的风险；必要时仅对对话框启用，主窗口使用不透明渐变。
- 偏好设置中的“玻璃模糊由 Wayland 合成器提供”改为按平台显示；Windows 文案说明由系统 DWM 提供或不可用。
- 验收：原生标题栏暗色、最大化/全屏/迷你播放器/置顶、双显示器不同缩放、任务栏图标与托盘。

### P3 — 系统媒体控制（可选，约 2–3 天）

- 最小可用：`RegisterHotKey` 注册全局 `VK_MEDIA_*`，与现有 QAction 共用逻辑；注销放在 RAII 析构。
- 进阶：WinRT `SystemMediaTransportControls`（元数据、播放状态、进度、Seek、打开/关闭），替代 MPRIS 的角色。MSYS2 需 `mingw-w64-x86_64-cppwinrt`，若工具链受限则记录为已知缺口。
- 可选：`ITaskbarList3` 任务栏缩略图按钮；Qt 6 已无 QtWinExtras，需少量 COM 代码。

### P4 — 打包、CI 与发布（约 2–3 天）

- `packaging/windows/nekoplayer.rc` + `nekoplayer.ico` + manifest（PerMonitorV2、Windows 10/11 兼容 GUID、`asInvoker`）。
- 部署脚本：
  - `windeployqt --release --compiler-runtime` 收集 Qt 与 MinGW 运行库；
  - 用 `ldd`/`objdump -p` 收集 `libmpv-2.dll`、FFmpeg 及其依赖；
  - 生成 `nekoplayer-<version>-windows-x86_64.zip`，内含 `LICENSE` 与第三方依赖说明。
- 发布：`.github/workflows/release.yml` 增加 Windows job，与源码包一起上传；预发布标记逻辑沿用 tag 规则。
- 许可证：MSYS2 的 mpv/FFmpeg 构建通常启用 GPL 组件，分发 DLL 时必须提供对应源码获取方式；本项目 GPL-3.0-only 与之一致，但需在打包文档写清。
- 代码签名：无证书时 SmartScreen 会警告；是否购买证书由发布策略决定，先记录为已知边界。

### P5 — 测试与验收矩阵（持续）

- CI：构建 + 确定性 smoke（`code-size`、`media-source`、`m3u8-store`、`playlist-file`、`playlist-block-move`、会话存储等），媒体/窗口套件保留为桌面验收。
- 真实 Windows 验收清单：
  - 安装/便携启动、单实例转发、“打开方式”与文件关联；
  - H.264/HEVC/AV1 软解与 d3d11va 硬解、HDR 色调映射（非原生输出）；
  - 双字幕、外挂字幕、字幕样式、截图与剪贴板；
  - 全屏/迷你/置顶、双显示器不同缩放、托盘、任务栏图标；
  - 会话续播、Unicode/长路径、UNC 网络共享、网络 URL；
  - 退出/崩溃后无残留进程与锁文件。

## CMake 草案

```cmake
find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core Concurrent Widgets OpenGLWidgets LinguistTools Network)
find_package(Qt6 6.5 QUIET COMPONENTS DBus)

if(UNIX AND Qt6DBus_FOUND)
    target_sources(neko_ui PRIVATE src/desktop/mprisplayer.cpp)
    target_link_libraries(neko_ui PUBLIC Qt6::DBus)
    target_compile_definitions(neko_ui PUBLIC NEKOPLAYER_HAS_MPRIS)
endif()

if(WIN32)
    target_sources(NekoPlayer PRIVATE packaging/windows/nekoplayer.rc)
endif()

if(UNIX)
    install(FILES data/nekoplayer.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES data/nekoplayer.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES data/nekoplayer.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
```

## 风险与边界

- **Qt DBus 在 Windows 不可用**：MPRIS 必须由 SMTC 替代；在 SMTC 完成前，Windows 版只有托盘与前台媒体键。
- **工具链一致性**：libmpv/FFmpeg 与 Qt 必须使用同一 MinGW 运行时；混用 MSVC 构件会导致 DLL/ABI 问题。
- **OpenGL 驱动**：远程桌面、虚拟机与旧显卡可能只有 GLES/ANGLE；需要在目标机器上验证，必要时评估 D3D11 渲染路径。
- **窗口透明**：这是 Windows 上最可能返工的外观项，先保证不透明可读，再谈 Mica/Acrylic。
- **原生 HDR**：当前渲染路径不提供 Windows HDR 直通；与 Linux 的 HDR 验收边界一致，需单独立项。
- **签名与安装器**：首版便携 zip 不涉及安装器/签名，但 SmartScreen 提示会存在。
- **无法在本机验证**：本机无 MinGW/MSVC；所有 Windows 结论必须由 CI + 真机验收补证。

## 下一步

1. 推送后观察 `windows-msys2` 首次构建，修正 MSYS2 依赖名称、`mpv.pc`/FFmpeg 查找与 Qt 模块差异；
2. 真机验收暗色标题栏、不透明窗口阴影/圆角与 QSS 面板；
3. 实现 Windows 全局媒体键（`RegisterHotKey`）与可选 SMTC；
4. 统一对话框的透明属性策略，评估 Win11 Mica/Acrylic 与运行时主题更新；
5. 补齐安装器/文件关联/签名与便携包验收。
