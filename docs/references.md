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

媒体信息字段参考 IINA `iina/InspectorWindowController.swift` 的 `updateInfo()`：容器格式、当前视频编码、容器帧率、音频编码及解码参数。语义对照 [mpv v0.41.0 input.rst](https://github.com/mpv-player/mpv/blob/v0.41.0/DOCS/man/input.rst) 的 `file-format`、`file-size`、`current-tracks`、`video-params`、`audio-params`、`container-fps` 与 `hwdec-current`。容器帧率可能不准确，文件大小是媒体源长度；本机原生 Matroska 解复用器报告 `mkv`。NekoPlayer 独立实现拥有数据的快照、异步事件补全与非模态 Widgets 纯文本报告，不采用 IINA 的定时同步查询，也未复制其 GPL 源码。

逐帧参考 Haruna `src/qml/Actions.qml` 的 `onFrameStepForwardAction()`／`onFrameStepBackwardAction()`，以及 IINA `MenuController.swift` 的菜单映射。具体命令与局限对照 [mpv v0.41.0 input.rst](https://github.com/mpv-player/mpv/blob/v0.41.0/DOCS/man/input.rst) 的 `frame-step`／`frame-back-step`；前进原生播放一帧后暂停，后退执行精确定位，不能用于纯音频。结合 mpv `player/command.c` 的当前轨道属性和 `player/playloop.c` 的 `add_step_frame()`，使用事件观察区分动态视频与静态图片，保留原生定位和解码行为。NekoPlayer 独立实现类型化控制器和共享 Widgets 动作，没有复制上游代码；无界面测试核对 25 fps 素材实际 40 ms 步进，不推定 Wayland 画面或硬解已验证。

A–B 循环参考 Haruna `src/qml/Actions.qml` 的端点动作与 `src/mpv/mpvitem.cpp` 的 `reset-on-next-file`，具体语义对照 [mpv v0.41.0 options.rst](https://github.com/mpv-player/mpv/blob/v0.41.0/DOCS/man/options.rst) 的 `ab-loop-a`／`ab-loop-b`：缺少任一端点禁用循环，手动定位越过 B 不强制回跳。NekoPlayer 独立实现类型化端点、异步提交、边界校验和 Qt Widgets 菜单，不采用 Haruna 的同步属性写入，也未复制其 GPL 代码。原生回跳和文件级重置／恢复以本机 libmpv 的无界面测试验证。

随机播放参考 Haruna `src/playlist/playlistmodel.cpp` 的 `shuffleIndexes()`：将随机访问索引与列表显示顺序分离，并保留当前项锚点。本项目独立实现持久索引候选池、有界实际访问历史和异步失败恢复，没有复制 Haruna 的 GPL 源码或引入 KDE 依赖；移动、重复路径、轮次交界和错误重入以本项目回归测试验证。

音轨/字幕阶段参考了 mpv 的 `DOCS/man/input.rst` 中 `track-list`、`aid` / `sid` 和 `sub-add` 说明、本机 `mpv/client.h` 的 node 数据所有权约定，以及 Haruna `src/mpv/mpvitem.cpp` 的轨道加载流程。本项目独立实现类型化快照与异步接口，没有复制这些项目的源码，也没有引入 Haruna 的 Qt Quick 依赖。

会话恢复参考了 Haruna `src/settings/generalsettings.kcfg`、`audiosettings.kcfg`、`playlistsettings.kcfg` 对音量、文件对话框目录、窗口 geometry 与队列偏好的划分。使用 Qt 自带 QSettings 独立实现，没有复制 KDE 配置代码或引入 KDE 依赖。损坏配置的重复解析问题对照了 [Qt qsettings.cpp](https://github.com/qt/qtbase/blob/v6.10.0/src/corelib/io/qsettings.cpp) 中 `QConfFile::fromName`、`syncConfFile` 和 `ensureAllSectionsParsed`，并在本机 Qt 6.12 以回归测试验证。

续播参考 Haruna `src/mpv/mpvitem.cpp` 的 `loadFile()`：在加载前提供 `start` 可避免播放开始后再定位的开销。本项目使用每次异步 `loadfile` 的文件级选项，不设置会影响后续文件的全局 `start`。对照 mpv `player/loadfile.c` 的初始定位与 `player/playloop.c` 的 `handle_loop_file()`，并以真实 libmpv 测试确认续播后的原生单曲循环回到媒体开头。持久条目身份、单次消费及 QSettings 保存逻辑均独立实现，未复制参考项目源码；本阶段只保存上次会话选中项，不引入完整 watch-later 文件库。

倍速／延迟操作参考 Haruna `src/qml/Actions.qml` 的动作划分及 mpv 手册中的 `speed`、`audio-delay`、`sub-delay` 和 `audio-pitch-correction`。本机 mpv 0.41 的延迟选项仍使用 float，倍速使用 double，参见 [v0.41.0 options/options.c](https://github.com/mpv-player/mpv/blob/v0.41.0/options/options.c)；当前克隆的 master 不代表本机版本。属性观察对不变值不重复发事件，确认与数据所有权以本机 `mpv/client.h` 的异步 set/get 和观察契约为准。本项目独立实现共享 Qt Widgets 动作和异步确认，没有复制参考项目源码。

播放模式参考 Haruna `src/mpv/mpvitem.cpp` 的 `endOfFileAction()`、`onEndOfFileReached()` 对重复当前项和重复队列的区分，以及 mpv `DOCS/man/options.rst` 中 `loop-file` 与 `loop-playlist` 的差异。NekoPlayer 仍由应用管理多文件队列，单文件使用异步设置的 mpv 原生循环；`player/playloop.c` 的 `handle_loop_file()` 说明该路径在自然 EOF 时定位，而非重载文件。相关行为在本机 mpv 0.41 的真实非 GUI 集成测试中验证；未复制上游实现。

章节与时间定位参考 Haruna `src/qml/Actions.qml` 的上下章动作，以及 mpv `DOCS/man/input.rst` 中 `chapter-list`、`chapter` 和精确定位的契约。对照 `player/command.c` 的 `mp_property_chapter()` 与 `player/playloop.c` 的 `get_current_chapter()`，章节属性设置会先记录目标意图，越过末章可能结束播放；本项目在控制器内校验边界，并以异步确认处理连续点击。独立实现 Qt Widgets 菜单、拥有数据的章节快照和时间解析，没有复制上游源码或引入 Qt Quick。

兼容性复核对照了 [mpv v0.41.0 的 playloop.c](https://github.com/mpv-player/mpv/blob/v0.41.0/player/playloop.c) 与忽略目录中的上游 master：0.41 在普通定位覆盖待执行原生选章时可能留下 `last_chapter_flag=true` 和 `last_chapter_seek=-2`，暂停时 `chapter` 持续不可用；master 已将相关重置移到 `queue_seek()`。NekoPlayer 不修改系统 mpv，改为使用章节快照中的起点提交精确定位，通过命令回复、SEEK／PLAYBACK_RESTART 和异步查询确认实际章节；真实回归覆盖覆盖定位及相同章节起点。

硬解设置参考 Haruna `src/qml/Settings/PlaybackSettings.qml` 和 `src/settings/playbacksettings.kcfg` 的解码选项与回退提示，以及 mpv `DOCS/man/input.rst` 的 `hwdec`／`hwdec-current` 区分。前者是可写偏好，后者才是实际使用方式；写入可能重建解码器并定位刷新。NekoPlayer 保留安全自动模式与软件模式两个常用入口，独立实现 Qt Widgets 应用／取消和 schema 1 可选字段，未复制参考源码。测试以本机 mpv 0.41 行为为准，null 视频输出不能验证 GPU 解码互操作。

字幕显示参考 Haruna `src/qml/Settings/SubtitlesSettings.qml` 的字体选择与恢复默认入口，以及 `src/settings/subtitlessettings.kcfg` 的独立字幕偏好。具体选项以 [mpv v0.41.0 options.rst](https://github.com/mpv-player/mpv/blob/v0.41.0/DOCS/man/options.rst) 中 `sub-visibility`、`sub-scale`、`sub-pos`、`sub-font` 和 `sub-ass-override` 为准：隐藏保留选择与解码，100 是原始位置而非绝对屏幕底部，默认字体通常不覆盖 ASS，缩放／移动可能影响 ASS 排版。本项目独立实现 Qt Widgets 菜单、草稿对话框和类型化偏好，没有复制 Haruna 的 GPL 源码或引入 Qt Quick／KDE 依赖；保留 mpv 默认 ASS 样式策略。

列表编辑参考 Haruna `src/playlist/playlistproxymodel.cpp` 与 `playlistfilterproxymodel.cpp` 的移动／删除职责划分。原生拖放对照 [Qt 6.5.3 qlistview.cpp](https://github.com/qt/qtbase/blob/v6.5.3/src/widgets/itemviews/qlistview.cpp) 的 `dropEvent()` 和 [qabstractitemview.cpp](https://github.com/qt/qtbase/blob/v6.5.3/src/widgets/itemviews/qabstractitemview.cpp) 的 `startDrag()`：InternalMove 优先调用 model 的 moveRow，已移动的拖放不会再次删除源项。本项目独立实现单层队列移动、持久索引与共享编辑动作，没有复制 Haruna 源码或引入代理模型依赖。

字幕拖放参考 mpv.net `src/MpvNet/Player.cs` 的 `LoadFiles` 中按扩展名选择 `sub-add` 或 `loadfile` 的分流，以及 `FileTypes.cs` 的字幕扩展名列表。本项目独立实现 Qt Core 拖放路由；扩展名与已有字幕对话框保持一致，并在发出操作前校验整批 URL。为避免将字幕挂到旧影片，本阶段明确拒绝混合媒体与字幕或多个字幕的拖放，没有复制 mpv.net 源码。

画面控制参考 Haruna `src/qml/Actions.qml` 的 `onRotateClockwiseAction()`／`onRotateCounterClockwiseAction()`，只提供四分之一圈旋转。具体语义以本机 mpv 0.41 手册和 [v0.41.0 options.rst](https://github.com/mpv-player/mpv/blob/v0.41.0/DOCS/man/options.rst) 的 `video-aspect-override`、`keepaspect` 和 `video-rotate` 为准：`no` 清除比例覆盖，旋转 0 仍应用方向元数据，无回拷硬解只支持 90° 倍数。[v0.41.0 m_option.c](https://github.com/mpv-player/mpv/blob/v0.41.0/options/m_option.c) 的 `parse_double_aspect()` 将 `no` 存为 -2，属性读取返回数值，测试按此校验。对照 mpv `video/out/aspect.c` 与 `video/out/gpu/video.c` 的画面矩形和纹理变换，本项目仅提交原生选项，独立实现类型化控制器和 Qt Widgets 共享菜单，没有复制上游代码或引入 CPU 滤镜；null 视频输出测试不能证明 GPU 路径的实际效果。
