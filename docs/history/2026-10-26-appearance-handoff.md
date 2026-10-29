> **历史存档（2026-10-26）**：本文记录当次外观收敛交接，其中的测试数量、窗口测试策略和未提交文件说明已被后续工作更新。当前状态以 `README.md`、`docs/verification.md` 和 `docs/audit/2026-10-28.md` 为准。

# 外观与 UI 半透明效果开发交接

更新时间：2026-10-26
仓库：`/home/nekorain/Development/NekoPlayer`
分支：`main`
基础外观实现提交：`6aa897e`。本轮 Apple UI 收敛最新代码基线：`555a663`（`main` 与 `origin/main` 同步）。

## 收束状态

本阶段的 UI 实现与自动化回归已交付，用户决定在此收束开发。当前安装版与 `build/debug/NekoPlayer` 二进制 SHA-256 一致；最近一次非窗口 Debug CTest 为 70/70 通过。窗口行为测试按用户要求未运行。

原生 Wayland 已有局部实机观察：niri、Qt Wayland、eDP-1 2880×1800@120 Hz、1.75×；一次空首页/全屏检查中，内容居中且 transport 固定于底部。此证据不代表完整视觉矩阵通过。Light、玻璃关闭、其他强调色、100% 缩放、真实播放/OSD、播放列表、设置和弹出菜单、mini-player、焦点/禁用态均未完成系统化检查。后续如恢复开发，应先补这些验收；当前不再以此为由继续扩展代码。

这次收束不宣称所有人工 Wayland 验收标准均已满足，也不覆盖 HDR 输出、硬件解码或合成器 blur 验证。播放器进程 PID `2055146` 属于用户当前会话，交接时应保留；工作区中 `docs/skills.md` 是用户已有未提交修改，不属于本阶段提交。

## 1. 交接结论

外观偏好阶段已实现并提交。用户可选择 Dark、Light、System 三种基础主题，Ocean、Emerald、Violet、Rose、Amber 五种强调色，以及 UI 玻璃式半透明样式开关。偏好页支持即时预览、Apply 持久化和 Cancel 回滚；Settings 菜单中的旧主题菜单继续可用，并与新偏好页共用状态。整体审查发现 System 主题原先没有把强调色写进原生 palette role；该修正已提交为 `332f08d`，并包含在最终验证中。

本轮进一步完成统一 AppearanceTokens/QSS 状态契约、偏好设置/播放列表/首页/Transport 的尺寸与密度，以及 fullscreen transport 使用统一 160 ms 动效 token；新增 Ctrl+K 快速操作面板，从实时 QAction 树过滤并触发现有命令，不复制第二份命令清单。面板支持上下键从搜索输入进入结果列表，新增界面文案已补齐英文翻译（325 条已完成、0 条未完成）。具体验证结果见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。

“玻璃”在本实现里指 UI 控件的半透明渐变、边缘明暗和主题色表面，不是真正的背景模糊。视频帧、libmpv 渲染、硬件解码和 HDR 路径没有为了外观效果而改动。最新 Debug 全量构建通过；非窗口 CTest 为 66/66，定向 sanitizer 子集为 7/7。窗口行为测试只编译，未按项目要求运行。

## 2. 已交付内容与提交

本阶段代码范围从 `1a645de`（计划提交）起；外观实现至 `6aa897e`，交接说明提交为 `a41468d`。后续修正：

| 提交 | 内容 |
| --- | --- |
| `a273ead` | 强调色枚举、外观 tokens、玻璃/不透明样式基础 |
| `e091006` | 外观配置读写、启动应用和主题菜单状态 |
| `2d46cd9` | 偏好设置外观页、即时预览、Apply/Cancel、英文翻译 |
| `6aa897e` | UI 表面覆盖和视频/全屏反馈隔离回归测试 |
| `332f08d` | System 保留平台标准调色板，同时应用强调色选择/链接 role；补充回归测试 |
| `4cd7108` | 统一偏好设置入口：F5/F6/设置菜单共用 `PreferencesCoordinator`，保留当前外观等值并统一 Apply/Cancel |
| `eea57a7` | 四级材质 token、边框/尺寸 token 与统一 QSS 交互状态 |
| `d7954ce` | 设置、播放列表、首页卡片和 Transport 布局密度统一 |
| `48f1a23` | fullscreen transport 淡入淡出改用共享动效时长，并补状态样式断言 |
| `010ced6` | 新增 Ctrl+K QAction 快速操作面板 |
| `7c77d76` | 补齐设置外边距与欢迎卡片高度回归测试 |
| `995a5b5` | 快速操作面板支持从搜索框用上下键切换结果 |

主要实现位置：

- [appearancepreferences.h](/home/nekorain/Development/NekoPlayer/src/ui/appearancepreferences.h) / `.cpp`：外观值类型、设置序列化、应用到 QApplication、关于信息。
- [appearancetokens.h](/home/nekorain/Development/NekoPlayer/src/ui/appearancetokens.h) / `.cpp`：Dark/Light/System 的语义颜色、强调色派生和 QPalette。
- [appearancestylesheet.cpp](/home/nekorain/Development/NekoPlayer/src/ui/appearancestylesheet.cpp)：基于 token 生成 Widgets QSS；透明面板规则集中在 `panelFill()`。
- [preferencesdialog.cpp](/home/nekorain/Development/NekoPlayer/src/ui/preferencesdialog.cpp)：Appearance 分类、可访问控件名、即时预览和回滚。
- [appearancemenu.cpp](/home/nekorain/Development/NekoPlayer/src/ui/appearancemenu.cpp)：共享主题 QAction、进程内设置状态和允许配置时的持久化。
- [main.cpp](/home/nekorain/Development/NekoPlayer/src/main.cpp)：启动时读入并应用外观设置；`--no-config` 使用默认进程内值且不持久化。
- 测试：`tests/appearance_preferences_test.cpp`、`tests/preferences_dialog_test.cpp`、`tests/ui_surface_test.cpp`。

普通手写代码文件以 500 行为目标，自动检查允许少量超出至 550 行。少数明确列入 `cmake/CheckCodeSize.cmake` 的窗口协调/桌面集成文件可不超过 800 行，新增例外必须附带理由；不能把限制当作目标。本轮 `preferencesdialog.cpp` 为 527 行、`windowactions.cpp` 为 550 行、`ui_surface_test.cpp` 为 547 行，均在硬限制内；之后应优先将设置页协调逻辑与测试拆分，避免继续增长。

## 3. 设计和运行契约

### 外观设置模型

`AppearancePreferences::Settings` 包含：

- `theme`：`Dark`、`Light` 或 `System`。
- `accent`：`Ocean`、`Emerald`、`Violet`、`Rose` 或 `Amber`。
- `glassEnabled`：UI 半透明样式开关，缺省为启用。

QSettings 键固定为：

| 键 | 值/默认值 | 无效/缺失行为 |
| --- | --- | --- |
| `appearance/theme` | `dark` / `light` / `system`，默认 `dark` | 未识别值回到 Dark |
| `appearance/accent` | `ocean` / `emerald` / `violet` / `rose` / `amber`，默认 `ocean` | 未识别值回到 Ocean |
| `appearance/glassEnabled` | bool，默认 `true` | 缺失时启用 |

改设置时应调用 `AppearancePreferences::loadSettings/saveSettings/applySettings`，不要在控件里重复实现键解析或调色板构造。`System` 基于 Qt 当前 style 的 `standardPalette()` 取底色，但仍套用用户所选强调色和玻璃样式。`--no-config` 下新外观可在当前运行期间使用，但不得写入用户配置文件。

### UI 与视频边界

玻璃样式覆盖欢迎页/历史操作行、播放列表、菜单、对话框和其他非播放控制面板；普通与全屏 transport 固定使用相同的不透明 `surface` token，不受玻璃开关影响。`VideoWidget` 不应被 QSS 透明化、安装 graphics effect 或变更渲染属性。`PlaybackOverlay` 与 `ActionFeedbackOverlay` 保留其各自的 OSD 表面语义，但外观切换不能给它们增加图形效果。全屏 transport 自己现有的 opacity/fade 动画必须继续独立工作。

Qt Widgets 没有可移植的同窗兄弟控件 backdrop-filter；QSS 也没有可移植 box-shadow。当前渐变下缘只是轻量阴影感模拟。Wayland 合成器的窗口外 blur 即使存在，也只影响顶层窗口背后的桌面区域，并不能模糊本窗口内部视频或相邻控件。因此 UI 开关和文案不得声称“背景模糊”。不要仅为此功能引入 Vulkan、GPU blur renderer、视频帧复制或新的渲染路径。

## 4. 当前验证证据

本次发布准备轮次运行的命令和结果：

| 检查 | 结果 |
| --- | --- |
| `cmake --preset debug` | 成功 |
| `cmake --build --preset debug --clean-first -j4` | 成功；全部目标（含 `window_test`）重新构建，翻译资源重新生成 |
| `LC_ALL=C LANG=C ctest --test-dir build/debug --output-on-failure -E '(window|^file-startup$|^preferences-dialog$|^video-color-dialog$)'` | **54/54 通过，57.50 秒** |
| 聚焦 Debug 选择（`code-size`、偏好、外观、UI、时间条、交互策略、会话、文件夹、标题、列表编辑） | **11/11 通过，6.56 秒** |
| 聚焦 ASan/UBSan 选择（偏好对话框/协调器、交互策略、时间条、会话、文件夹菜单） | **6/6 通过，2.46 秒**；设置 `ASAN_OPTIONS=detect_leaks=0` 以排除平台库退出分配，不宣称泄漏检查通过 |
| System palette 强调色 role 修正 | 已提交 `332f08d`，定向与全量非窗口测试均通过 |
| `ctest --test-dir build/debug -R '^code-size$' --output-on-failure` | **1/1 通过**（0.33 秒） |
| 本阶段 40 个 C++ 文件的 `clang-format --dry-run --Werror` | 通过 |
| `git diff --check -- . ':(exclude)docs/skills.md'` | 通过 |
| 翻译目录 | `播放时间` → `Playback time` 已补充；`lrelease` 报告 277 finished / 0 unfinished |
| Task 4 独立只读审查 | 无 Critical、Important、Minor 发现；判定 Ready |

整分支审查补充发现 System palette 未采用 accent 的 Highlight/HighlightedText/Link role（P2）。该修正已以测试优先方式提交为 `332f08d`：保留 standard palette 的其余 role，并覆盖上述三项 accent role。定向测试与最终非窗口套件（54/54）均通过。审查还指出设计规格提到 soft shadows，但当前 QSS 没有真实投影。Qt Widgets QSS 无可移植 `box-shadow`；当前只有渐变下缘的轻量明暗模拟，因此此项作为明确的设计限制记录，不额外添加逐控件 graphics effect 或第二条绘制路径。

非窗口测试集合明确排除了含 `window` 的目标，以及 `file-startup`、`preferences-dialog`、`video-color-dialog`。这是按项目既有验证记录使用的选择；本次完整结果不能替代偏好对话框交互或原生桌面行为验收。Task 3 之前对偏好页定向测试曾通过 5/5，但那不是本次全量回归结果。

`windowactions.cpp` 的格式检查在早期交接时曾报告基线遗留告警；发布准备轮次对 `332f08d..ebfbb21` 的 40 个变更 C++ 文件执行 `clang-format --dry-run --Werror` 已全部通过。

以下内容没有被本轮测试证明：

- 原生 Wayland 下主题/玻璃实际观感、缩放/DPI、弹出菜单和全屏动画的最终合成结果。
- 真正的窗口内背景模糊（本功能没有实现此能力）。
- HDR 显示器输出、色彩管理、GPU 硬解和驱动兼容；本阶段不触碰这些路径。
- 真实播放器视频画面是否在所有桌面环境保持稳定居中；单元/离屏绘制测试不能证明这一点。

## 5. 已知缺口与后续优先级

### 已解决：统一偏好设置入口（`4cd7108`）

此前 `SubtitleDisplayMenu` 的“字幕显示设置”会自行创建 `PreferencesDialog`，没有把当前 `AppearancePreferences::Settings` 传给新对话框，也没有连接 `appearancePreferencesChanged`；从该入口修改主题可能只改变预览，重启后不保留。

现在 F5、F6 和“设置”菜单都通过 `PreferencesCoordinator` 构造共享对话框，打开时读取当前外观、媒体库、续播位置、定位步长和交互值，Apply 统一经 `AppearanceMenu::setSettings()` 应用并按 `configurationEnabled` 持久化，Cancel 回滚。`preferences-coordinator`、`preferences-dialog`、`appearance-preferences` 与 `ui-surface` 测试覆盖当前值传递与 Apply 信号；`--no-config` 的会话缺失回退没有自动化运行证据，窗口套件中的无会话断言仅编译、不执行。该缺口已关闭。

### P1：安排一次原生 Wayland 视觉验收

在可用 Wayland 桌面记录 compositor、Qt platform plugin、屏幕缩放和分辨率。至少对 Dark/Light、玻璃开/关、代表性 accent 检查欢迎页/历史、播放列表、QMenu/QDialog、窗口 transport 和全屏 transport；额外检查窄窗口、焦点/禁用态、文字对比度、视频中心和播放区域几何不因底栏而跳动。留存不含隐私的截图或明确记下“未验收”。如果只能用 offscreen/Xvfb，结果只能写作布局/控件测试，不能写作 Wayland 视觉通过。

### P2：检查系统主题运行时变化和平台 fallback

目前 `System` 在调用 `applySettings()` 时读取 Qt standard palette。后续如果产品要求系统主题变化时不重启即可跟随，需要确认 Qt/platform style 是否发出足够可靠的 palette/style 变化通知，并设计重建 token/QSS 的生命周期；在完成前不要把“System”描述为一定支持桌面主题变化热更新。需在目标 Wayland style 与 Fusion/offscreen fallback 上做可复现测试。

### P2：校验翻译和可访问性文案

外观控件提供 objectName/accessibilityName，英文目录已添加新文案。发布准备轮次补充了 `播放时间` → `Playback time`，最终 `lrelease` 报告 277 finished / 0 unfinished。继续增加强调色/材质说明或本地化时，应更新翻译 catalog 并验证其编译；玻璃解释需清楚写“半透明/柔化表面”，不要误译成真实背景模糊。验证键盘导航、焦点边框与浅色主题文字对比度。

### P3：若产品坚持真正的 blur，先做平台能力/设计评估

真正 blur 是独立的需求，不是当前玻璃开关的缺陷修复。先确认目标合成器、Wayland 协议/桌面扩展、窗口类型、视频不受影响的严格边界和 fallback，再做小型技术验证并测量耗电/帧时间。没有可移植且不影响视频的方案时，保留当前高性能渐变样式。

### 相关的产品级待办（不属于本次提交）

- 许可证尚未在项目层面选定；从 `references/` 借鉴或复制代码之前，先确定项目许可证并逐项目核对上游许可证与归属要求。外观阶段目前没有复制第三方实现。
- HDR 识别/色调映射与显示器/Wayland HDR 输出是不同层。真实 HDR 色彩管理仍要在目标 GPU、驱动、显示器与合成器组合上验证；不要用 UI 测试替代。
- 项目坚持单一 libmpv 播放/渲染路径；硬解选项“启用/自动”不等于运行时一定使用硬件解码，应以运行时 `hwdec-current` 和实际设备验证。

## 6. 接手建议

1. 先运行 `git status --short --branch`。当前共享工作区有一个用户已有的 `docs/skills.md` 修改，必须保留，不要重置、覆盖或顺手纳入外观提交。
2. 确认 `main` 已包含上述外观提交、统一入口（`4cd7108`）和发布准备提交；本项目不创建额外开发分支或 worktree。
3. 统一 Preferences 入口已由 `4cd7108` 完成。下一步优先安排在目标 Wayland 桌面做视觉与输入验收；若发现问题，再按窄范围补测试和修正。
4. 验收后按 `docs/verification.md` 的发布准备记录复核构建、非窗口套件、聚焦 sanitizer、格式和 diff 检查；分别报告自动化与人工证据。
5. 除非用户另行要求，不要改动 mpv、HDR、硬解或 OpenGL 呈现实现来追求 UI blur。

## 7. 本次交接文件自身状态

本文件记录外观功能的实现边界、验证和可执行后续项。它不代表所有产品功能完成，也不代表原生 Wayland/HDR/硬解验收完成。交接时保留已有的 `docs/skills.md` 工作区修改；本文件不应把该文件内容重写或吸收到提交中。

## 8. Apple UI 收敛后续

- 操作反馈浮层现复用统一 160 ms `OutCubic` 动效淡入淡出，并在 Qt 6.12+ 尊重系统 Reduce Motion；重复反馈在淡出期间到达会更新文案并继续显示。实现只改变浮层自身绘制透明度，不安装 `QGraphicsOpacityEffect`，也不触碰视频/播放路径。定向 Debug 与 ASan/UBSan 结果见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)；Wayland 实际合成观感仍待映射稳定后验收。
- 2026-10-26 又在 XWayland 下完成了单配置首页静态截图检查：欢迎入口与底部 transport 未相互覆盖，当前 1.75× 下可读；截图已删除。Qt Wayland 仍收到 configure 后进入 `suspended true`，niri 未枚举 NekoPlayer，generic platform theme 也不能改变结果；niri CLI 与正在运行的 compositor 版本不同。此次 XWayland 检查不是 Wayland 验收，Dark/Light、玻璃开关、100% 缩放、播放/设置/菜单、全屏、mini-player 和交互矩阵仍未完成。调查细节在 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。
- Ctrl+K 面板采用动作名称/快捷键的大小写无关包含与字符顺序模糊过滤：完整子串优先，其余按匹配间隔排序，仍直接消费实时 QAction，不维护镜像命令表；目前不按菜单层级分组，也不做拼写纠错。
- `ThumbnailPreview` 保持立即显示/隐藏：它会随时间轴 hover 快速更换帧，不增加逐帧 opacity 合成动画；当前共享时长只用于 fullscreen transport。若之后要动画预览浮层，先在 Wayland 窗口和 GL 视频共存路径上测量合成成本与延迟。
- 快速操作测试使用 offscreen Qt；需要在原生 Wayland 手工确认 Ctrl+K、方向键、Enter、Esc 与焦点回退。面板自身不实现背景模糊。
- 2026-10-26 的 Quick Actions 交互复核补齐了共享 QAction 去重、菜单上下文/图标、空结果提示、双击触发和异步焦点回退；验证记录见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。原生 Wayland 输入与实际观感仍待人工验收。
- 2026-10-26 后续补上字符顺序 fuzzy 搜索，子串优先、匹配间隔评分及稳定顺序回退；Debug、ASan/UBSan 定向回归通过，具体命令与范围见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。
- 全屏 transport 现在在 Qt 6.12+ 尊重系统减少动态效果偏好，偏好运行时切换为 Reduce Motion 时会即时完成当前淡出；旧 Qt 保留默认 160 ms。motion policy、UI surface 定向回归及完整非窗口 Debug 结果见验证记录。当前 niri 会话未能稳定映射 NekoPlayer 窗口，故没有截图或 Wayland 视觉通过结论。
- 时间轴缩略图预览现在复用同一 160 ms 动效 token，以 paint opacity 淡入/淡出，不安装 graphics effect，也不在每次 hover 帧更新时重启动画；Qt 6.12+ Reduce Motion 下即时显示/隐藏。`thumbnail-preview` 覆盖此行为；见验证记录。
- 当前可用环境没有完成 Dark/Light、玻璃开关、不同 accent、100%/175% 缩放、全屏视频几何与 mini-player 的原生组合验收。HDR、`hwdec-current`、音频设备与 compositor blur 仍是独立的硬件/桌面验收项；本轮不声称已通过。
- 早期 niri 映射复查曾失败（Qt 收到 `xdg_toplevel.configure` 后标记 `suspended true`，窗口未出现在 `niri msg windows`；软件 GL 重试相同），但 2026-10-26 显式以 `QT_QPA_PLATFORM=wayland` 启动 Debug 构建时，窗口已成功映射，旧的“当前无法映射”结论已过时。当前仅完成一次 Dark/Ocean 默认主题、玻璃开启、无媒体欢迎页的静态检查：eDP-1 2880×1800@120 Hz，Qt Wayland，niri，1.75×（1645×1028 逻辑像素）；应用当时在 805×1005 逻辑像素的平铺区域。没有与应用交互，未运行窗口行为测试。首页主入口、居中内容和底部 transport 在该画面中没有明显重叠/位置跳动。截图包含桌面其他窗口，因此没有保留或提交。该局部检查不代表 Dark/Light、accent、玻璃开关、100% 缩放、播放/播放列表、菜单/设置、全屏、mini-player 与焦点状态的完整验收。
- 四级 surface 后续复核发现弹出控件之前复用了普通 panel fill。现已将菜单、对话框、组合框弹出列表和 tooltip 映射到 `raisedSurface`，普通 transport/内容面板保留 `surface`；统一由 `panelFill()` 处理玻璃与不透明 fallback，不触碰视频渲染区域。定向 Debug 与 ASan/UBSan 结果见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。
- 对比度复核新增 Dark/Light 全表面 WCAG AA 测试，覆盖正文、次级和辅助文字在 canvas/surface/raised/hover/pressed 上的组合；发现并修正了浅色 muted 文字偏淡、深色 muted 文字偏暗的问题。合同测试单独位于 `appearance_surface_contract_test.cpp`，设置测试主体保持在 500 行以内。自动化结果见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)；原生合成后的对比度仍需 Wayland 屏幕检查。
- 键盘焦点复核给复选框 indicator 增加了基于 `focus` token 的 2px 边界，并由 surface contract 验证五种 accent 与 Dark/Light 核心表面至少 3:1；实际合成可见性仍需原生 Wayland 检查。
- 后续检查发现滑块焦点规则使用了 Qt QSS 不支持的 CSS `outline`。现改为 `QSlider::handle:horizontal:focus`：获得键盘焦点时增大手柄并显示 token 焦点边界，时间轴和音量滑块共用。一个真实 QSlider 的离屏聚焦前后像素回归曾按预期失败，修复后相关 Debug 3/3、ASan/UBSan 1/1 通过；CTest 固定使用 offscreen 平台以保证焦点测试可复现。该自动化结果不能替代原生 Wayland 的焦点显示检查，窗口行为测试仍按要求跳过。详细命令见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。
- 原生桌面静态复查：本次启动中曾观察到扫描进度浮窗，但第二次相同的无配置/无媒体启动未复现；静态调用路径中未发现启动自动扫描来源，未据此改代码或将其定性为产品缺陷。两次临时全屏截图含宿主桌面隐私内容，检查后均已删除。
- A3 状态复核发现输入控件缺少显式禁用样式，Qt 6.12 offscreen 上禁用 QLineEdit 前后绘制完全相同。现为组合框、字体组合框、行编辑和数值框增加基于语义 token 的禁用文字、背景和边框；真实控件回归同时覆盖滑块。测试先失败后通过，Debug 4/4、ASan/UBSan 1/1、格式与 code-size 检查通过，提交为 `60d4003`。半透明合成下的禁用态仍需 Wayland 检查。
- 后续状态覆盖发现 QCheckBox 禁用文字仍依赖 Qt palette，没有显式消费 `disabledText` token；勾选且禁用时的 indicator 也没有独立规则。现已在应用级 QSS 补上 token 化禁用文字，并保持 disabled checked indicator 的勾选符号。新增真实控件合同先按预期失败，修复后外观/UI 定向 5/5、ASan/UBSan surface contract 10/10、格式与 code-size 检查通过，详见 [verification.md](/home/nekorain/Development/NekoPlayer/docs/verification.md)。有效对比度仍需在原生 Wayland 半透明合成下确认。
- 另增加真实 QLineEdit/QComboBox/QSpinBox/QPushButton/QToolButton 键盘聚焦前后的像素变化契约，防止仅凭焦点 QSS 字符串断言可见。offscreen 测试容器最初自动将焦点给 line edit，导致基准帧已带焦点；调整为父容器持有基准焦点后通过五次重复运行。Debug 4/4 与 ASan/UBSan 1/1 均通过，测试单独提交 `01f560b`；它验证 Qt offscreen 样式行为，不替代 Wayland 实际焦点/键盘路径验收。
- 提交 `7e10ff1` 将播放信息与定位进度 OSD 的前景、raised 表面、分隔色和进度 accent 接入应用级 AppearanceTokens；玻璃关闭时 OSD 使用不透明 token fallback。未给 `PlaybackOverlay` 或 `VideoWidget` 设置局部 stylesheet/graphics effect，播放与 OpenGL 路径不变。离屏真实控件颜色回归与 appearance/UI 聚焦 Debug 5/5 通过；Wayland + libmpv 实帧合成效果仍未检查。
- 后续像素审计发现 OSD 透明度还未跟随设置页里的玻璃强度/柔化值；已将两个 OSD surface 统一接到应用 QSS 唯一 `panelFill()`，并新增不同 opacity 渲染结果必须变化的真实 QWidget 回归。离屏 Debug 聚焦套件通过，Wayland 视频帧合成仍未验收。
- 另一个独立的自绘操作反馈浮层（定位/音量/播放状态）现在从当前 palette 读取 raised 背景和文字色，解决浅色主题仍显示固定深色卡片的问题；它保持原有透明度与动效，不套用 UI 玻璃设置。真实 QWidget 像素测试在修复前失败、修复后通过。真实播放帧上的可读性仍待 Wayland 检查。
- 两项 OSD 收敛修复后的 fresh Debug 非窗口套件为 69/69；外观/两类 OSD 的 ASan/UBSan 定向测试为 4/4（`detect_leaks=0`）。本次环境中的 Qt Wayland 窗口仍未被 niri 枚举，因此设计要求的原生屏幕矩阵没有完成。
- 首页最近播放卡片的标题与位置文字间距经真实布局测量曾为 3 px；现改为共享的 4 px `spaceUnit`，对应真实 QWidget 几何回归先失败后通过。没有改动卡片总体层级或尺寸。
- 偏好设置各分类的表单标签现共用由 4 px spacing token 推导的 128 px 标签列，避免切换分类时输入列横向跳动；新布局回归曾测得 22 px 偏差并先失败后通过。Transport 在中等宽度下的控件改为严格遵循 36 px 正方形热区，紧凑态仍为 32 px。定向测试与自动化验证见 `docs/verification.md`；离屏几何不替代原生 Wayland 屏幕验收。
- 2026-10-26 新一轮原生启动诊断中，Qt Wayland `--smoke-test --silent-audio --no-config` 可成功退出，但持久窗口收到 `xdg_toplevel.configure` 后仍标记 `suspended true`，随后又收到 0×0 configure，niri 未枚举它。相同 session 下的 XWayland 应用窗口可以直接捕获并检查；具体结果与限制见 `docs/verification.md`。没有重启 compositor 或改桌面配置，Wayland 全矩阵仍待完成。
- 外观回归补查发现偏好设置 tab 可被键盘聚焦但没有焦点反馈：实际 QTabBar 焦点前后图像相同。应用级 QSS 已加入 token 焦点边框，新的真实控件像素测试先失败后通过；Debug 外观/UI 4/4、ASan/UBSan surface contract 1/1 通过。原生 Wayland 的焦点可见性仍需验收，细节见验证记录。
- 旧记录：全屏曾使用至少 90% 玻璃不透明度、普通播放栏跟随用户透明度；该行为已由后续收敛取代。当前普通/全屏 transport 均固定使用不透明 `surface` token，玻璃开关只影响其他 UI 面板；淡入淡出仍只改变可见度。
- 2026-10-26 transport 材质修正：真实 QWidget 像素测试复现玻璃开启时 transport alpha 为 185；QSS 原因是 transport 误用了共享的玻璃 `%27` 面板填充。现改为不透明 `surface` token，普通/全屏背景在玻璃开关两种状态下相同，且 alpha 为 255。`fullscreen-transport-material`、`appearance-surface-contract`、`ui-surface` 和 `code-size` 通过；不涉及视频帧、libmpv、HDR 或硬件解码路径。原生 Wayland 播放画面仍未完成全矩阵验收。
- 播放列表面板的玻璃开关现有独立真实 QWidget 像素契约：启用时 alpha 小于 255，关闭时 alpha 为 255 且填充改变；该断言确认玻璃设置仍作用于 UI，而不是简单全局关闭透明度。
- transport 另外有一条经过真实 Appearance 设置页的集成像素契约：切换 `appearanceGlassChoice` 会即时重建应用 QSS，而普通/全屏共用的 transport 始终不透明地绘制 `surface`。将样式映射故意改回玻璃 `panelFill()` 后，回归以 alpha 185 失败；恢复后实际设置链路通过。此测试验证 QWidget 样式，不验证合成器最终输出。最近非窗口 Debug CTest 69/69；人工 Wayland 外观矩阵仍未完成。
- 上述设置链路测试也已在 Sanitizers 构建通过 1/1（`ASAN_OPTIONS=detect_leaks=0`，不宣称 leak 检查）；格式与 code-size 门禁通过。人工 Wayland 合成观感仍待复核。
- Mini-player 转场现抽为 `WindowOpacityTransition`，复用全屏 transport/缩略图一致的 160ms `OutCubic` 和 Reduce Motion 策略；窗口 flags 与尺寸只在淡出完成后切换一次，再淡入恢复。没有逐帧缩放 OpenGL 视频面，也没有添加 graphics opacity effect。fresh Debug 非窗口 CTest 70/70，窗口行为测试未运行；原生 Wayland mini-player 连续性和完整外观矩阵仍需人工验收。
- 四类可选 UI 动效现在还共享唯一 `uiMotionEasingCurve()`（`OutCubic`），避免全屏 transport、缩略图、操作反馈和 mini-player 的曲线漂移；Debug 70/70、Sanitizer 定向 1/1。原生 Wayland 的实际动效观感仍需人工验收。
- 最新原生 Wayland 诊断实例观察 5 秒仍未出现在 niri 窗口列表，日志没有应用崩溃，仅有 AT-SPI 总线警告；诊断进程已结束，未修改桌面配置。该会话仍不能提供可信的原生 UI/mini-player 视觉证据。
- 另外修正了 mini-player 淡出期间立即进入全屏的状态竞态：全屏路径现在同步取消 pending opacity 转场并退出 mini-player，再进入全屏；普通 mini-player 切换仍使用统一淡入淡出。新增转场取消回归，Debug 非窗口 70/70、Sanitizer 1/1；真实 Wayland 合成序列仍未验收。
- 2026-10-26 安装版运行复查：`/usr/local/bin/NekoPlayer` 在显式 `QT_QPA_PLATFORM=wayland` 下成功映射到 niri；eDP-1 为 2880×1800@120 Hz、1.75×。全屏空状态截图中内容保持居中，transport 固定在底部且未观察到画面区域跳动；这只覆盖 Dark/Ocean、玻璃开启、无媒体的一种配置。随后桌面锁屏，Light/玻璃关闭/其他 accent/设置菜单/播放列表/mini-player/焦点态和 100% 缩放仍未完成人工验收。完整非窗口 Debug 回归在本次复查中通过 70/70，窗口行为测试仍按要求跳过。
- 2026-10-26 发现空/失败媒体状态的 `VideoWidget::paintGL()` 曾把玻璃透明度应用到 OpenGL 清屏 alpha，让 UI 玻璃设置穿透到视频 surface。已改为玻璃开启时仅取 palette 色并强制 alpha 255，玻璃关闭仍清为不透明黑色；活动视频帧仍由原有 libmpv GPU 路径渲染。offscreen 平台不支持可靠的 `QOpenGLWidget` framebuffer 断言，因此自动化覆盖集中在可确定的 clear-color 契约，真实合成仍需 Wayland 检查。修复后定向测试与完整非窗口 Debug 70/70 通过。真实 Wayland 的媒体播放像素/OSD、100% DPI 与其余外观组合依旧未完成验收。
