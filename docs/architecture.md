# NekoPlayer 工程分层

本阶段使用 C++20 / Qt 6 Widgets / 单一 libmpv；参考 PotPlayer 的紧凑桌面交互。

| 组件 | 职责 | 依赖边界 |
| --- | --- | --- |
| `MpvController` | client RAII、异步命令、属性观察、文件事件、播放状态和渲染恢复快照 | Qt Core / libmpv client API，不依赖窗口 |
| `PlaybackState` | 只读播放快照；秒为时间单位，音量 0–100 | 控制器与展示层共用 |
| `MediaTrack` / `mpvtracks.cpp` | 类型内唯一的轨道编号、元数据、选择状态；将 mpv node 解析为拥有数据的快照 | 领域类型不依赖 mpv；解析器只依赖 Qt Core / client API |
| `MediaChapter` / `mpvchapters.cpp` | 章节原始编号、标题及起始秒数；复制借用的 node 为独立快照 | 领域类型不依赖 mpv，解析器不依赖 UI |
| `playbacktime.cpp` | 时间字符串解析与格式化，校验格式、有限值及转换范围 | Qt Core，无播放或窗口状态 |
| `PlaylistModel` | 本地文件队列、当前项与原生单项移动；提供 Qt model 角色和内部拖放数据 | Qt Core，不直接控制播放器 |
| `PlaylistController` | 连接队列、播放控制器和 EOF/加载失败事件；顺序／循环策略与有界失败恢复 | 仅依赖 `MpvController` 与 `PlaylistModel` |
| `VideoWidget` | GL 上下文、物理像素 FBO、帧更新、隐藏时跳帧、资源释放 | 只处理渲染，不提交播放命令 |
| `TransportBar` | 展示时间/进度/音量，拖动预览，发出定位和音量意图 | 状态快照和 QAction，不操作 mpv |
| `PlaylistPane` | 队列视图、独立编辑选择、当前项标识及编辑动作入口 | 通过队列业务 API 和共享 QAction 操作，不接触 mpv |
| `TrackMenus` | 音轨、字幕菜单和外挂字幕文件入口 | 依赖控制器快照与业务接口，不接触 mpv handle |
| `TuningMenus` | 倍速、音频／字幕延迟预设、自定义输入和快捷键 | 共用 QAction，显示控制器确认的值，不接触 mpv handle |
| `PlaylistModeMenu` | 互斥播放模式动作、R 快捷键及模式标题 | 只调用队列业务接口，菜单与右键共用 |
| `NavigationMenu` / `TimeSeekDialog` | 章节动作、上下章及指定时间输入校验 | 只调用控制器业务接口，共享菜单与快捷键 |
| `PlaybackSettingsDialog` | 解码方式草稿、应用／取消与实际解码状态 | 仅调用控制器业务 API，QObject 接收上下文随对话框销毁 |
| `SubtitlePreferences` | 字幕可见性、缩放、位置及默认字体；统一归一化 | Qt Core 值类型，不依赖 mpv 或 UI |
| `SubtitleDisplayMenu` / `SubtitleSettingsDialog` | 共享字幕显示动作、快捷键和应用／取消草稿 | 只使用字幕偏好与控制器业务 API，不订阅播放进度 |
| `SessionState` / `SessionStore` | 音频偏好、窗口偏好和队列值类型；版本化 INI 校验与原子保存 | Qt Core，不依赖 UI 或 libmpv |
| `SessionManager` | 恢复空闲队列及音频偏好，采集会话、记住媒体目录 | 通过队列/播放业务 API，不调用 mpv |
| `MainWindow` | 组件组装、文件入口、错误显示、拖放、全屏和生命周期协调 | 通过队列和控制器接口发出业务操作 |
| `windowactions.cpp` | 同一 MainWindow 类的动作与菜单装配 | 菜单、按钮、快捷键共用 QAction |
| `windowsession.cpp` | 同一 MainWindow 类的会话装配、窗口 geometry 与关闭保存 | SessionManager、Qt Widgets，不介入帧渲染 |
| `main.cpp` | 参数解析、启动和 smoke 退出条件 | 后端诊断实现单独保留 |

业务路径：用户文件入口 → `PlaylistController` 选择有效本地项 → 控制器异步命令 → mpv 事件 → Qt 接收者队列 → 状态信号 → 展示更新。EOF 自动选择下一项；加载失败只跳过后续项，不将一般 mpv 错误误认为队列切换。渲染路径独立：mpv 更新回调 → Qt 队列 → GL current → render API → Qt 合成。

播放模式由队列持有，默认 Off，不写入会话配置。One 和只有一项的 All 通过控制器异步设置 `loop-file`，由 mpv 定位回起点，避免重载解码器和丢失轨道。多项 All 的 EOF／手动导航由应用队列首尾衔接；列表长度或模式改变时立即更新原生循环设置。若切换为 One 时 EOF 已排队，结束回调兜底重播当前项。每次导航最多扫描队列一轮，异步失败行和缺失行记录在失败集合中，错误链不会反复重试同一行。成功自然播完、显式导航或清空会重置失败集合；`FILE_LOADED` 仅代表容器和轨道初始化成功，不能据此清除失败历史。全失败后保留队列，追加或显式操作可重试。模式变化独立通知动作可用性，不订阅时间进度。

后台回调只排队，不访问 UI、不调用 mpv。带 QObject 接收上下文的排队任务在对象销毁时取消。控制器和表面各自的回调 userdata 保留到对应 mpv 资源完全释放。

轨道管理复用同一 client 的事件循环：观察 `track-list` → 在借用 node 失效前复制字符串和标志 → 快照变化时发送 `tracksChanged` → 重建共享菜单。音轨和字幕编号只在各自类型内唯一；控制器按类型校验编号，0 表示关闭，通过异步命令设置 `aid` / `sid`。外挂字幕路径作为独立 UTF-8 参数传给异步 `sub-add`，解析和文件读取交给 mpv。菜单不订阅时间进度来重建内容；播放状态更新只在 active 变化时更新可用性。停止、加载失败或开始换片会清除轨道快照，闲置状态忽略迟到的非空轨道数据。

控制器公开方法和初始化位于 `mpvcontroller.cpp`，私有事件／恢复实现在 `mpvcontroller_p.h`，不向 UI 暴露内部结构。倍速与延迟通过有限值校验、范围限制和异步属性设置提交；独立 `tuningChanged` 通知避免进度更新触发菜单刷新。每项保留最新请求供连续步进累积，最新设置回复后异步查询实际值，再清除待确认状态。新设置使旧查询失效，旧回复不能清除新请求；不依赖必然出现的属性变化事件，也不比较请求值与后端浮点值来判断完成。此路径无定时轮询、同步查询或配置写盘。倍速／延迟是本次运行的全局偏好，不随媒体状态清空。

章节操作集中在 `mpvcontroller_navigation.cpp`，复用同一 client 和事件循环。观察 `chapter-list` / `chapter`，只在快照变化时发 `chaptersChanged`；章节菜单不随时间进度重建，动作更新缓存章节与可定位状态。控制器先按快照校验编号，再异步精确定位到章节起点，规避 mpv 0.41 原生选章被普通定位覆盖后章节属性不可用的竞态。待确认目标供快速连续步进累积；`chapterNavigationIndex` 与独立意图信号及时更新上下章动作的边界，章节勾选仍使用实际观察值。最新命令回复后异步查询章节，同值定位可立即确认；否则等待该命令回复后的 SEEK 和 PLAYBACK_RESTART，再查询最终章节。章节共用起点或帧对齐可能落到其他章节，此时以最终结果清除待确认目标。查询编号及完成标志共同隔离旧回复，发信号前完成状态变更，避免重入取消新请求。普通定位、停止、换片与播放重置取消待确认目标。时间对话框提交前重新检查当前路径、可定位状态及时长；精确定位使用 `absolute+exact`，帧显示遵循媒体时间戳。停止、失败或开始换片清除章节快照，不增加解码器、渲染器或持续轮询。

销毁顺序：停止窗口定时器 → 删除 central widget / VideoWidget → 撤销 render 回调 → GL current 下释放 renderer → 撤销 client 回调 → 销毁 client。主窗口显式删除 central widget，避免 QMainWindow 基类析构晚于控制器成员的问题。

上下文重建：表面发出即将重建信号 → 控制器保存当前文件、位置、暂停状态及轨道快照 → renderer 释放 → 新 GL context 创建 renderer → 控制器异步重新加载文件并恢复状态。外挂字幕按路径去重，并在 FILE_LOADED 后逐个异步加载，收到完成回复后才处理下一个。全部完成后异步查询轨道列表，按内部编号或外挂文件内的流顺序恢复音轨/字幕的选择或关闭状态；不能仅依赖加载选项，因为 mpv 会在轨道布局变化时重置选择。恢复这一文件时禁用字幕自动发现，避免同一文件重复加入。连续重建时保留尚未完成的目标快照，忽略旧恢复请求的回复；用户手动选择轨道或添加字幕会取消待完成的恢复。正常全屏切换不重新挂载视频控件；测试通过跨窗口重新挂载主动触发 context 重建。

普通手写代码文件最多 500 行（含头文件、测试、CMake 和测试启动脚本）。`src/ui/mainwindow.cpp` 和 `tests/window_test.cpp` 是已登记的集中协调/桌面集成例外，最多 800 行；例外列表由 `cmake/CheckCodeSize.cmake` 强制执行，新增项必须有明确理由。新增职责拆成独立组件；同一类的紧密相关装配代码可以分文件，不为拆文件增加无用途的抽象。保持 Qt parent ownership 与 RAII 的资源归属唯一。

会话只在启动读取、关闭保存，不订阅时间进度写盘。音量和静音偏好订阅控制器的用户意图信号，避免异步初始属性通知或刚启动就关闭时把已保存值覆盖为默认值。队列通过 restoreFiles 恢复但不播放；窗口播放入口在 renderer ready 后启用。渲染就绪前的待播路径也纳入退出快照，清空动作同时清除它们；渲染失败保留它们供下次启动。MainWindow 关闭时保存，未经过 closeEvent 的析构提供一次兜底保存。

QSettings schemaVersion=1，原子同步开启；保存前独立检查现存文件。Qt 按路径缓存解析结果，而 FormatError 属于具体 QSettings 实例，因此校验把文件字节复制到唯一临时文件后完整解析，避免第二次访问丢失先前的格式错误。未知版本或解析失败拒绝覆盖；路径仅接受绝对本地路径，缺失文件保留，标量域值归一化。正常窗口 geometry 通过 Qt 跨平台 API 保存，全屏下保存进入全屏前的 geometry，不跨启动恢复全屏。默认路径在 main 决定，smoke/no-config 和默认构造的测试窗口禁用会话读写。

解码偏好由控制器记录成功提交到同一 mpv client 的最新用户意图，`hardwareDecodingEnabled` 不代表实际使用硬解；运行时异步设置 `hwdec=auto-safe/no`，相同偏好不重复提交。`PlaybackState::hardwareDecoder` 继续观察 `hwdec-current`，区分未播放、无视频解码器、软件及具体硬解 API。设置对话框缓存 active/decoder，只在相关值变化时刷新标签，不增加轮询或逐帧配置写盘。SessionStore 在 schema 1 增加可选 `video/hardwareDecoding`，缺失／非法值默认 true；SessionManager 关闭时读取即时意图，启动在加载媒体前应用。异步命令错误通过现有错误信号报告，硬件不可用时的正常回退不改变用户偏好。

目前没有播放进度持久化、随机模式、完整偏好设置界面、插件框架、多引擎或完整主题系统。

字幕显示实现集中在 `mpvcontroller_subtitles.cpp`，复用同一 client 的异步 `set` 命令；只有变更的字段才提交，UTF-8 字体作为独立参数传递。`SubtitlePreferences` 表示最新成功入队的用户意图，快速步进和关闭保存直接读取它，不等待属性观察；异步执行错误仍由现有错误信号报告，不宣称后端已执行成功。显示开关使用 `sub-visibility`，不改 `sid`；缩放为 0.25–4，位置为 0–100，拒绝非有限步进；字体去除首尾空白，空值、控制字符或超长名称回退 `sans-serif`。不设置 ASS 强制覆盖，不新增帧复制、轮询或媒体重载。schema 1 的可选 `subtitles/*` 字段缺失或损坏时分别回退默认值；停止、换片和渲染重建不清除偏好。字幕菜单仍按媒体状态启用，空闲设置入口由全局设置菜单和共享 F6 动作提供。对话框只在打开时枚举字体，保留泛型及当前未安装的字体名称；偏好变化通知驱动动作刷新，无逐帧处理。

列表编辑沿用 Qt 的 `beginMoveRows`／`endMoveRows` 与 QListView InternalMove，不重载媒体。内部 MIME 对象持有 `QPersistentModelIndex`，跨移动和删除跟踪真实条目，区分重复路径并拒绝外部或已失效数据。当前行和失败行集合随结构变更映射；删除当前项先停止并取消自动续播，再选择邻项，迟到 EOF／加载错误不能自行启动它。面板通过持久索引区分编辑选择与播放项，仅播放项身份改变才同步选择；代理用 CurrentRole 加粗，不让 model 依赖字体。编辑 QAction 由面板提供给列表、按钮和主菜单，快捷键仅在列表聚焦时生效；不订阅播放进度，不新增帧处理或配置写盘。

缺失文件的同步错误回调也允许修改队列：首次报错前才为剩余扫描候选建立持久索引快照，按原遍历顺序最多尝试每个仍存在的候选一次，回调中新追加的项留给下一次导航。正常有效文件入口不构建整表快照。新扫描、清空或移除当前项会使旧扫描修订号失效；取消与耗尽是不同结果，取消不能发出 queueFinished 或覆盖重入的新播放请求。
