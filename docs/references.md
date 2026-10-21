# 参考项目

以下项目已浅克隆到 `references/`，保持各自独立的 Git 仓库。它们是学习材料，不是构建依赖。复用源码前须核对具体文件及依赖的许可证。

| 项目 | 上游 | 克隆提交（短 SHA） | 重点参考 |
| --- | --- | --- | --- |
| mpv | https://github.com/mpv-player/mpv | a43ab319fdb7 | client/render API、选项与生命周期 |
| mpv-examples | https://github.com/mpv-player/mpv-examples | e0d1a84c99e8 | 嵌入式播放与渲染示例 |
| MpvQt | https://github.com/KDE/mpvqt | 556d67f0f557 | Qt 与 mpv 桥接、事件和 OpenGL 生命周期 |
| Haruna | https://github.com/KDE/haruna | 87d9e20c8094 | Qt 播放器菜单、快捷键、播放列表与设置 |
| mpv.net | https://github.com/mpvnet-player/mpv.net | ef45baecbdd8 | 接近传统 Windows 播放器的操作与配置体验 |
| IINA | https://github.com/iina/iina | 7f2c76731285 | 桌面播放体验、字幕/音轨交互与产品细节 |
| KikoPlay | https://github.com/KikoPlayProject/KikoPlay | 08c81ee15809 | Qt 中文桌面交互、字幕、播放列表与弹幕生态 |

优先阅读 mpv-examples 和 MpvQt 的嵌入边界，再看 Haruna 的桌面交互。mpv.net、IINA 的平台和语言不同，主要借鉴产品行为。不要为了兼容所有参考项目而同时引入多种 UI 或播放内核。

官方 API 文档：https://mpv.io/manual/stable/ 。使用本机头文件确认可用接口；浅克隆 HEAD 不等于本机已安装发行版。

音轨/字幕阶段参考了 mpv 的 `DOCS/man/input.rst` 中 `track-list`、`aid` / `sid` 和 `sub-add` 说明、本机 `mpv/client.h` 的 node 数据所有权约定，以及 Haruna `src/mpv/mpvitem.cpp` 的轨道加载流程。本项目独立实现类型化快照与异步接口，没有复制这些项目的源码，也没有引入 Haruna 的 Qt Quick 依赖。

会话恢复参考了 Haruna `src/settings/generalsettings.kcfg`、`audiosettings.kcfg`、`playlistsettings.kcfg` 对音量、文件对话框目录、窗口 geometry 与队列偏好的划分。使用 Qt 自带 QSettings 独立实现，没有复制 KDE 配置代码或引入 KDE 依赖。损坏配置的重复解析问题对照了 [Qt qsettings.cpp](https://github.com/qt/qtbase/blob/v6.10.0/src/corelib/io/qsettings.cpp) 中 `QConfFile::fromName`、`syncConfFile` 和 `ensureAllSectionsParsed`，并在本机 Qt 6.12 以回归测试验证。

倍速／延迟操作参考 Haruna `src/qml/Actions.qml` 的动作划分及 mpv 手册中的 `speed`、`audio-delay`、`sub-delay` 和 `audio-pitch-correction`。本机 mpv 0.41 的延迟选项仍使用 float，倍速使用 double，参见 [v0.41.0 options/options.c](https://github.com/mpv-player/mpv/blob/v0.41.0/options/options.c)；当前克隆的 master 不代表本机版本。属性观察对不变值不重复发事件，确认与数据所有权以本机 `mpv/client.h` 的异步 set/get 和观察契约为准。本项目独立实现共享 Qt Widgets 动作和异步确认，没有复制参考项目源码。

播放模式参考 Haruna `src/mpv/mpvitem.cpp` 的 `endOfFileAction()`、`onEndOfFileReached()` 对重复当前项和重复队列的区分，以及 mpv `DOCS/man/options.rst` 中 `loop-file` 与 `loop-playlist` 的差异。NekoPlayer 仍由应用管理多文件队列，单文件使用异步设置的 mpv 原生循环；`player/playloop.c` 的 `handle_loop_file()` 说明该路径在自然 EOF 时定位，而非重载文件。相关行为在本机 mpv 0.41 的真实非 GUI 集成测试中验证；未复制上游实现。

章节与时间定位参考 Haruna `src/qml/Actions.qml` 的上下章动作，以及 mpv `DOCS/man/input.rst` 中 `chapter-list`、`chapter` 和精确定位的契约。对照 `player/command.c` 的 `mp_property_chapter()` 与 `player/playloop.c` 的 `get_current_chapter()`，章节属性设置会先记录目标意图，越过末章可能结束播放；本项目在控制器内校验边界，并以异步确认处理连续点击。独立实现 Qt Widgets 菜单、拥有数据的章节快照和时间解析，没有复制上游源码或引入 Qt Quick。

兼容性复核对照了 [mpv v0.41.0 的 playloop.c](https://github.com/mpv-player/mpv/blob/v0.41.0/player/playloop.c) 与忽略目录中的上游 master：0.41 在普通定位覆盖待执行原生选章时可能留下 `last_chapter_flag=true` 和 `last_chapter_seek=-2`，暂停时 `chapter` 持续不可用；master 已将相关重置移到 `queue_seek()`。NekoPlayer 不修改系统 mpv，改为使用章节快照中的起点提交精确定位，通过命令回复、SEEK／PLAYBACK_RESTART 和异步查询确认实际章节；真实回归覆盖覆盖定位及相同章节起点。

硬解设置参考 Haruna `src/qml/Settings/PlaybackSettings.qml` 和 `src/settings/playbacksettings.kcfg` 的解码选项与回退提示，以及 mpv `DOCS/man/input.rst` 的 `hwdec`／`hwdec-current` 区分。前者是可写偏好，后者才是实际使用方式；写入可能重建解码器并定位刷新。NekoPlayer 保留安全自动模式与软件模式两个常用入口，独立实现 Qt Widgets 应用／取消和 schema 1 可选字段，未复制参考源码。测试以本机 mpv 0.41 行为为准，null 视频输出不能验证 GPU 解码互操作。
