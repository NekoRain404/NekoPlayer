# 外观与 UI 半透明效果开发交接

更新时间：2026-10-24
仓库：`/home/nekorain/Development/NekoPlayer`
分支：`feat/first-playable`
外观实现提交：`6aa897e`。交接文档提交：`a41468d`。System palette 强调色 role 修正已提交为 `332f08d`；发布准备轮次已完成最终非窗口验证（54/54），但原生 Wayland 视觉验收仍未进行。

## 1. 交接结论

外观偏好阶段已实现并提交。用户可选择 Dark、Light、System 三种基础主题，Ocean、Emerald、Violet、Rose、Amber 五种强调色，以及 UI 玻璃式半透明样式开关。偏好页支持即时预览、Apply 持久化和 Cancel 回滚；Settings 菜单中的旧主题菜单继续可用，并与新偏好页共用状态。整体审查发现 System 主题原先没有把强调色写进原生 palette role；该修正已提交为 `332f08d`，并包含在最终验证中。

“玻璃”在本实现里指 UI 控件的半透明渐变、边缘明暗和主题色表面，不是真正的背景模糊。视频帧、libmpv 渲染、硬件解码和 HDR 路径没有为了外观效果而改动。最终 Debug 构建通过，标准非窗口 CTest 集合通过 54/54；窗口行为测试只编译，未按项目要求运行。

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

主要实现位置：

- [appearancepreferences.h](/home/nekorain/Development/NekoPlayer/src/ui/appearancepreferences.h) / `.cpp`：外观值类型、设置序列化、应用到 QApplication、关于信息。
- [appearancetokens.h](/home/nekorain/Development/NekoPlayer/src/ui/appearancetokens.h) / `.cpp`：Dark/Light/System 的语义颜色、强调色派生和 QPalette。
- [appearancestylesheet.cpp](/home/nekorain/Development/NekoPlayer/src/ui/appearancestylesheet.cpp)：基于 token 生成 Widgets QSS；透明面板规则集中在 `panelFill()`。
- [preferencesdialog.cpp](/home/nekorain/Development/NekoPlayer/src/ui/preferencesdialog.cpp)：Appearance 分类、可访问控件名、即时预览和回滚。
- [appearancemenu.cpp](/home/nekorain/Development/NekoPlayer/src/ui/appearancemenu.cpp)：共享主题 QAction、进程内设置状态和允许配置时的持久化。
- [main.cpp](/home/nekorain/Development/NekoPlayer/src/main.cpp)：启动时读入并应用外观设置；`--no-config` 使用默认进程内值且不持久化。
- 测试：`tests/appearance_preferences_test.cpp`、`tests/preferences_dialog_test.cpp`、`tests/ui_surface_test.cpp`。

普通手写代码文件以 500 行为目标，自动检查允许少量超出至 550 行。少数明确列入 `cmake/CheckCodeSize.cmake` 的窗口协调/桌面集成文件可不超过 800 行，新增例外必须附带理由；不能把限制当作目标。本阶段 `windowactions.cpp` 当时为 462 行；外观测试文件后续有增长，添加测试前仍优先拆分。

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

玻璃样式覆盖欢迎页/历史操作行、播放列表、菜单、对话框和 transport 表面；关闭后这些表面由主题 token 输出不透明颜色。`VideoWidget` 不应被 QSS 透明化、安装 graphics effect 或变更渲染属性。`PlaybackOverlay` 与 `ActionFeedbackOverlay` 保留其原有局部透明背景语义，但外观切换不能给它们增加图形效果。全屏 transport 自己现有的 opacity/fade 动画必须继续独立工作。

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
2. 确认 `feat/first-playable` 上已经包含上述外观提交、统一入口（`4cd7108`）和发布准备提交。不要自行 merge 或 push。
3. 统一 Preferences 入口已由 `4cd7108` 完成。下一步优先安排在目标 Wayland 桌面做视觉与输入验收；若发现问题，再按窄范围补测试和修正。
4. 验收后按 `docs/verification.md` 的发布准备记录复核构建、非窗口套件、聚焦 sanitizer、格式和 diff 检查；分别报告自动化与人工证据。
5. 除非用户另行要求，不要改动 mpv、HDR、硬解或 OpenGL 呈现实现来追求 UI blur。

## 7. 本次交接文件自身状态

本文件记录外观功能的实现边界、验证和可执行后续项。它不代表所有产品功能完成，也不代表原生 Wayland/HDR/硬解验收完成。交接时保留已有的 `docs/skills.md` 工作区修改；本文件不应把该文件内容重写或吸收到提交中。
