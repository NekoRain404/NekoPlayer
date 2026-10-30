# 更新日志

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 的结构，版本号遵循语义化版本；预发布使用 `-rcN` 后缀。发布附件与校验和由 `.github/workflows/release.yml` 生成。

## [Unreleased]

### 计划
- 原生 Wayland HDR、硬件解码与多显示器实机验收。
- 字幕在线搜索与下载（当前明确推迟）。
- Windows 构建与打包。

## [0.2.0-rc1] - 2026-10-30

### 新增
- 媒体库：监视文件夹、递归扫描、搜索、后台刷新与取消，索引持久化到 `library.ini`。
- 快捷键编辑：F1 参考对话框内设置、清除、恢复默认，按作用域校验冲突并持久化。
- 字幕样式：文字/描边/背景颜色、描边宽度、阴影与编码。
- 音频/视频效果：10 段均衡器预设、锐化、降噪，去隔行保持独立选项。
- 按媒体记忆倍速、音频延迟与字幕延迟；睡眠定时器。
- 单实例转发、图片媒体（PNG/JPEG/WebP/BMP/GIF/AVIF/HEIC）与 URL 凭据隐藏。
- M3U8 保留 `#EXTINF` 时长与标题，删除/排序/移动后元数据跟随条目。
- 新增 Trash、Close、MiniPlayer 运行时图标；媒体库与快捷键界面完成英文本地化。

### 变更
- 源码按职责重组：`playback/{mpv,media,playlist,render}`、`ui/{window,dialogs,menus,panels,theme}`；测试镜像同一分层，注册模块迁入 `tests/cmake/`。
- README 重构为带横幅、真实截图和架构图的概览；逐项行为迁入 `docs/features.md`。
- 版本号提升到 0.2.0，新增标签驱动的源码发布工作流；CI 在 Arch 容器中执行完整构建与核心 smoke，完整媒体/窗口套件由桌面预设验收。

### 修复
- 文件夹扫描进度对话框在空闲启动时误显示；改为首次扫描时懒创建，并补窗口测试。
- 播放器停止后截图失败时，用户错误流与完成信号的处理不一致；测试期望与实现对齐。
- 事件队列耗尽后的重入/恢复测试依赖单次 `processEvents()`；改为有界重试，消除并行测试抖动。
- 重新整理后修复 code-size 例外路径、测试 include 与 CMake 源列表。

### 安全
- 持久化继续使用原子写、schema 版本与 0600 权限；网络地址在媒体信息中隐藏账号与查询参数。
- 单实例 socket 保持在每用户运行时目录，不落入共享临时目录。

### 验证
- `ctest --preset debug -j 6`：**89/89** 通过（含窗口集成）。
- X11 预设保留 `xvfb-run` + Openbox 回归；详见 `docs/verification.md`。

## [0.1.0] - 2026-10-26

### 新增
- 首个可播放版本：Qt Widgets + QOpenGLWidget + libmpv render API。
- 播放/暂停/停止/定位、音量与静音、全屏、章节、逐帧、A–B 循环。
- 音轨与字幕选择、外挂音轨/字幕、画面比例/旋转、色彩调整、倍速与延迟。
- 播放列表、文件夹扫描、M3U8 读写、会话续播、最近播放与媒体信息。
- 深色外观与玻璃质感、偏好设置、窗口置顶/小窗、Arch `PKGBUILD`。

[Unreleased]: https://github.com/NekoRain404/NekoPlayer/compare/v0.2.0-rc1...HEAD
[0.2.0-rc1]: https://github.com/NekoRain404/NekoPlayer/compare/v0.1.0...v0.2.0-rc1
[0.1.0]: https://github.com/NekoRain404/NekoPlayer/releases/tag/v0.1.0
