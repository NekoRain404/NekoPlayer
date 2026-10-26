# Apple 风格 UI 收敛 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 NekoPlayer 的玻璃材质、布局密度、交互反馈和键盘操作入口收敛为一套 Apple 风格、Wayland 友好的 Qt Widgets UI。

**Architecture:** `AppearanceTokens` 是颜色、表面、尺寸和动效的唯一契约，`appearanceStyleSheet()` 在应用级生成 QSS；组件只消费 token 或由应用级 QSS 获得状态样式，不自行解释玻璃参数。新增 `QuickActionsDialog` 以 MainWindow 的实时 `QAction` 树为数据源，复用现有动作、图标、快捷键和 enabled 状态，不建立第二份命令表。

**Tech Stack:** C++20、Qt 6 Widgets/Test、现有 QOpenGLWidget + libmpv 路径、CMake/CTest。

**Spec:** `docs/superpowers/specs/2026-10-26-apple-ui-convergence-design.md`

## Global Constraints

- 目标平台是 Wayland；保持 Qt Widgets + QOpenGLWidget + libmpv，不引入 Qt Quick、Vulkan、第二播放引擎或皮肤系统。
- 玻璃只作用于 UI 和窗口外合成器能力；不把视频帧复制到 CPU 做模糊，不给 VideoWidget、PlaybackOverlay 或 ActionFeedbackOverlay 安装玻璃样式。
- 普通代码文件不超过 500 行，已有明确例外最多 800 行；新增文件保持单一职责。
- 不运行窗口行为测试；可编译窗口测试，但只能把 Debug 非窗口 CTest 作为自动化验收证据。
- 每个任务完成后运行定向测试、`git diff --check`、code-size/clang-format 检查，并独立提交；不 merge、不 push、不使用 worktree。
- 保留用户已有的 `docs/skills.md` 修改；本计划不覆盖、不重置、不纳入提交。

## File Map

- `src/ui/appearancetokens.h/.cpp`: 四级表面、边框、焦点、控件高度、间距和动效 token，以及 palette 映射。
- `src/ui/appearancestylesheet.cpp`: 应用级 QSS；仅在这里解释玻璃透明度/柔化程度。
- `src/ui/preferencesdialog.cpp`, `src/ui/preferencesappearancepage.cpp`: 设置页布局密度、页签和按钮组契约。
- `src/ui/playlistpane.cpp`, `src/ui/welcomepanel.cpp`, `src/ui/transportbar.cpp`: 播放列表、首页卡片和 transport 的统一尺寸与可访问交互。
- `src/ui/quickactionsdialog.h/.cpp`: Ctrl+K 快速操作面板；只负责动作快照、过滤、选择和触发。
- `src/ui/mainwindow.h`, `src/ui/windowactions.cpp`: 创建并注册快速操作 QAction，管理对话框生命周期和焦点回退。
- `tests/appearance_preferences_test.cpp`, `tests/ui_surface_test.cpp`, `tests/playlist_pane_test.cpp`, `tests/transport_bar_test.cpp`: 视觉契约和布局回归。
- `tests/quick_actions_dialog_test.cpp`, `tests/ui_tests.cmake`: 快速操作行为测试及 CTest 注册。
- `docs/verification.md`, `docs/appearance-handoff.md`: 新鲜验证证据、已知 Wayland 人工验收缺口和后续工作。

### Task 1: AppearanceTokens 与 QSS 四级材质契约

**Files:**
- Modify: `src/ui/appearancetokens.h`
- Modify: `src/ui/appearancetokens.cpp`
- Modify: `src/ui/appearancestylesheet.cpp`
- Test: `tests/appearance_preferences_test.cpp`
- Test: `tests/ui_surface_test.cpp`

**Interfaces:**
- Produces `AppearanceTokens::accentSurface`, `border`, `focusRing`, `controlHeight`, `rowHeight`, `menuItemHeight`, `panelRadius`, `motionDurationMs` and their dark/light/system values.
- Keeps `appearanceStyleSheet(const AppearanceTokens &, bool, int, int)` as the single QSS entry point; `panelFill()` remains the only function that consumes glass opacity/blur.

- [x] **Step 1: Write the failing token contract tests**

Add tests that assert all themes expose non-null canvas/surface/raised/accent surfaces, that accentSurface changes with each accent while canvas/surface remain stable, and that control/row/menu metrics are multiples of four with positive values. Add a UI-surface test that renders a normal panel and verifies its stylesheet contains the token-derived focus and disabled rules while VideoWidget remains unstyled.

```cpp
void appearanceTokensExposeStableSurfaceHierarchy() {
    const auto base = appearanceTokens(AppearancePreferences::Theme::Dark);
    QVERIFY(base.canvas.isValid());
    QVERIFY(base.surface.isValid());
    QVERIFY(base.raisedSurface.isValid());
    QVERIFY(base.accentSurface.isValid());
    QVERIFY(base.controlHeight >= base.rowHeight);
    QCOMPARE(base.controlHeight % base.spaceUnit, 0);
    QCOMPARE(base.rowHeight % base.spaceUnit, 0);
    QVERIFY(base.focusRing == base.accent);
}
```

- [x] **Step 2: Run the focused tests and verify they fail**

Run: `cmake --build build --target appearance_preferences_test ui_surface_test -j2 && ctest --test-dir build -R 'appearance-preferences|ui-surface' --output-on-failure`

Expected: compilation fails because the new token fields and hierarchy assertions are not present.

- [x] **Step 3: Implement the token contract**

Add the fields with defaults in the header; populate them in `darkTokens()`, `lightTokens()`, and the System palette branch. Derive `accentSurface` from the canvas/accent mix, derive `border` from separator, and use one metric set (`4/8/12/16/24`) for all themes. Update QSS to map canvas/surface/raised/accent explicitly, use `border` for separators, `focusRing` for every focus selector, and `disabledText` for disabled controls. Remove newly duplicated hard-coded surface colors from selectors; keep video and playback overlays untouched.

- [x] **Step 4: Run focused tests and style checks**

Run: `cmake --build build --target appearance_preferences_test ui_surface_test -j2 && ctest --test-dir build -R 'appearance-preferences|ui-surface' --output-on-failure`; then `clang-format --dry-run --Werror src/ui/appearancetokens.h src/ui/appearancetokens.cpp src/ui/appearancestylesheet.cpp tests/appearance_preferences_test.cpp tests/ui_surface_test.cpp`.

Expected: PASS; rendered glass and opaque branches differ, and no VideoWidget stylesheet/effect changes.

- [x] **Step 5: Commit**

```bash
git add src/ui/appearancetokens.h src/ui/appearancetokens.cpp src/ui/appearancestylesheet.cpp tests/appearance_preferences_test.cpp tests/ui_surface_test.cpp
git commit -m "feat: unify apple surface tokens and qss"
```

### Task 2: 设置页、播放列表、首页与 Transport 的布局密度

**Files:**
- Modify: `src/ui/preferencesdialog.cpp`
- Modify: `src/ui/preferencesappearancepage.cpp`
- Modify: `src/ui/playlistpane.cpp`
- Modify: `src/ui/welcomepanel.cpp`
- Modify: `src/ui/transportbar.cpp`
- Test: `tests/playlist_pane_test.cpp`
- Test: `tests/transport_bar_test.cpp`
- Test: `tests/ui_surface_test.cpp`

**Interfaces:**
- Consumes Task 1 metrics through `appearanceTokens()`/application palette; no widget adds a second glass opacity or blur calculation.
- Preserves `TransportBar::timelineGlobalGeometry()`, `PlaylistPane::setSingleClickActivation(bool)`, and all existing QAction wiring.
- Produces stable object names and minimum sizes for settings rows, playlist search/header, welcome cards, transport controls, and narrow layouts.

- [x] **Step 1: Add failing geometry assertions**

Extend `transport_bar_test.cpp` to assert the timeline, primary button and time label use the shared minimum height. Extend `playlist_pane_test.cpp` to assert the search field has an accessible name and the view keeps a usable minimum row height. Add a narrow-width test that resizes the pane to 280 px and verifies the search field remains visible without a negative width.

```cpp
void narrowPlaylistKeepsSearchAndRowsUsable() {
    PlaylistController queue;
    QAction append(QStringLiteral("添加")), previous(QStringLiteral("上一个")),
        next(QStringLiteral("下一个")), clear(QStringLiteral("清空"));
    PlaylistPane pane(queue, &append, &previous, &next, &clear);
    pane.resize(280, 360);
    pane.show();
    QCoreApplication::processEvents();
    auto *search = pane.findChild<QLineEdit *>(QStringLiteral("playlistSearchEdit"));
    QVERIFY(search);
    QVERIFY(search->isVisible());
    QVERIFY(search->minimumSizeHint().height() >= 28);
}
```

- [x] **Step 2: Run the focused tests and verify the new assertions fail**

Run: `cmake --build build --target playlist_pane_test transport_bar_test ui_surface_test -j2 && ctest --test-dir build -R 'playlist-pane|transport-bar|ui-surface' --output-on-failure`.

Expected: at least one new metric/accessibility assertion fails against the current per-widget spacing.

- [x] **Step 3: Apply shared spacing and density**

Use the 4/8/12/16/24 scale for page margins, form row spacing, tab height, button groups, playlist header/search padding, welcome recent/continue card gaps, and transport groups. Set explicit object names and accessible names for icon-only transport controls. Keep the video area as the expanding stretch; do not insert a layout row below the video in fullscreen. Ensure narrow playlist content uses elision and keeps search/clear actions reachable.

- [x] **Step 4: Verify geometry, keyboard traversal, and formatting**

Run the focused CTests again, `ctest --test-dir build -R 'playlist-pane|transport-bar|ui-surface' --output-on-failure`, and clang-format on all modified files. Do not run `window-*` tests.

- [x] **Step 5: Commit**

```bash
git add src/ui/preferencesdialog.cpp src/ui/preferencesappearancepage.cpp src/ui/playlistpane.cpp src/ui/welcomepanel.cpp src/ui/transportbar.cpp tests/playlist_pane_test.cpp tests/transport_bar_test.cpp tests/ui_surface_test.cpp
git commit -m "feat: align ui layout density across surfaces"
```

### Task 3: 统一交互状态、焦点、提示和动效

**Files:**
- Modify: `src/ui/appearancestylesheet.cpp`
- Modify: `src/ui/playlistpane.cpp`
- Modify: `src/ui/transportbar.cpp`
- Modify: `src/ui/welcomepanel.cpp`
- Modify: `src/ui/fullscreencontrols.cpp`
- Modify: `src/ui/thumbnailpreview.cpp`
- Test: `tests/ui_surface_test.cpp`
- Test: `tests/interaction_policy_test.cpp`

**Interfaces:**
- Reuses existing `FullscreenControls`, `ThumbnailPreview`, and `ActionFeedbackOverlay` semantics; only timing, focus visibility, tooltips, and accessible names change.
- Produces consistent default/hover/pressed/focus/checked/disabled QSS states and a single 140–180 ms reveal/fade duration for UI overlays.

- [x] **Step 1: Add failing state and accessibility tests**

Add assertions for focus selectors, disabled selectors, and tooltip padding in the generated stylesheet. Add tests that icon-only controls have non-empty accessible names/tooltips and that fullscreen controls and thumbnail preview use the same duration constant.

```cpp
void stylesheetContainsCompleteInteractiveStates() {
    const auto tokens = appearanceTokens(AppearancePreferences::Theme::Dark);
    const auto qss = appearanceStyleSheet(tokens, true, glassOpacityDefault, glassBlurDefault);
    QVERIFY(qss.contains(QStringLiteral(":hover")));
    QVERIFY(qss.contains(QStringLiteral(":pressed")));
    QVERIFY(qss.contains(QStringLiteral(":focus")));
    QVERIFY(qss.contains(QStringLiteral(":disabled")));
}
```

- [x] **Step 2: Run focused tests and verify the new tests fail**

Run: `cmake --build build --target ui_surface_test interaction_policy_test -j2 && ctest --test-dir build -R 'ui-surface|interaction-policy' --output-on-failure`.

Expected: the new duration/accessibility checks fail before implementation.

- [x] **Step 3: Implement the shared interaction contract**

Add one token duration and use it for fullscreen/thumbnail reveal animations; preserve reduced-motion-safe instant transitions when the platform reports reduced motion. Add tooltips and accessible names to icon-only controls, ensure keyboard focus is visible over light and dark surfaces, and keep menu/combobox/search keyboard navigation intact. Do not install opacity effects or stylesheets on VideoWidget or playback OSD widgets.

- [x] **Step 4: Verify state transitions and formatting**

Run the focused tests, `ctest --test-dir build -R 'ui-surface|interaction-policy' --output-on-failure`, and clang-format on modified files. Check that no new per-frame polling or repaint loop is introduced.

- [x] **Step 5: Commit**

```bash
git add src/ui/appearancestylesheet.cpp src/ui/playlistpane.cpp src/ui/transportbar.cpp src/ui/welcomepanel.cpp src/ui/fullscreencontrols.cpp src/ui/thumbnailpreview.cpp tests/ui_surface_test.cpp tests/interaction_policy_test.cpp
git commit -m "feat: unify ui interaction states and motion"
```

### Task 4: Ctrl+K QuickActionsDialog

**Files:**
- Create: `src/ui/quickactionsdialog.h`
- Create: `src/ui/quickactionsdialog.cpp`
- Modify: `src/ui/mainwindow.h`
- Modify: `src/ui/windowactions.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/quick_actions_dialog_test.cpp`
- Modify: `tests/ui_tests.cmake`

**Interfaces:**
- `QuickActionsDialog(QObject *actionRoot, QWidget *parent = nullptr)` scans `actionRoot->findChildren<QAction *>()`, excludes separators and itself, and deduplicates by pointer.
- `void refreshActions()` rebuilds the visible model from current action text/icon/shortcut/enabled state.
- `QLineEdit *searchEdit() const`, `QListWidget *resultsView() const`, and `int visibleActionCount() const` expose testable state without exposing internal storage.
- Enter/double-click emits `QAction::trigger()` only for enabled actions; Escape closes; closing restores the widget that had focus before opening.

- [x] **Step 1: Write the failing dialog tests**

Create a root `QWidget` with actions named Open, Seek, Disabled, and a duplicate menu reference. Assert case-insensitive filtering, duplicate removal, enabled state presentation, Enter/double-click triggering, Escape closing, Ctrl+K shortcut registration, and focus restoration.

```cpp
void filtersLiveActionsAndTriggersEnabledOnly() {
    QWidget root;
    QAction open(QStringLiteral("打开文件"), &root);
    QAction disabled(QStringLiteral("不可用"), &root);
    disabled.setEnabled(false);
    QSignalSpy triggered(&open, &QAction::triggered);
    QuickActionsDialog dialog(&root);
    dialog.show();
    dialog.searchEdit()->setText(QStringLiteral("文件"));
    QCOMPARE(dialog.visibleActionCount(), 1);
    QTest::keyClick(dialog.searchEdit(), Qt::Key_Return);
    QCOMPARE(triggered.size(), 1);
    QVERIFY(dialog.isHidden());
}
```

- [x] **Step 2: Run the new test and verify it fails**

Run: `cmake --build build --target quick_actions_dialog_test -j2 && ctest --test-dir build -R quick-actions-dialog --output-on-failure`.

Expected: CMake cannot build the target because the dialog and registration do not exist yet.

- [x] **Step 3: Implement the dialog and MainWindow integration**

Use a compact non-modal `QDialog` with a search line edit and list widget, raisedSurface styling from the application stylesheet, no blur renderer, and no video overlap beyond the dialog geometry. Add `quickActionsAction_` to MainWindow with `Ctrl+K`, create it after the other actions, and open/reuse one dialog instance. Pass MainWindow as the action root, refresh immediately before showing, remember the previous focus widget, and restore it on close. Keep disabled actions visible but non-triggerable. Add object names, accessible names, arrow-key navigation, Enter, double-click, Escape, and a clear empty-state label.

- [x] **Step 4: Verify action-tree behavior and formatting**

Run: `cmake --build build --target quick_actions_dialog_test NekoPlayer -j2 && ctest --test-dir build -R 'quick-actions-dialog' --output-on-failure`; run the existing shortcuts and UI surface tests; run clang-format and code-size checks. Do not run window behavior tests.

- [x] **Step 5: Commit**

```bash
git add src/ui/quickactionsdialog.h src/ui/quickactionsdialog.cpp src/ui/mainwindow.h src/ui/windowactions.cpp CMakeLists.txt tests/quick_actions_dialog_test.cpp tests/ui_tests.cmake
git commit -m "feat: add keyboard first quick actions dialog"
```

### Task 5: 全量非窗口验证、Wayland 记录与交接文档

**Files:**
- Modify: `docs/verification.md`
- Modify: `docs/appearance-handoff.md`
- Optionally modify: `tests/appearance_preferences_test.cpp`, `tests/ui_surface_test.cpp` only for evidence gaps found by the commands below.

**Interfaces:**
- Consumes the four completed UI tasks and records exact commands/results, not inferred success.
- Produces a clear list of remaining Wayland display, mixed-DPI, HDR-output, hardware-decoder and compositor-blur checks.

- [x] **Step 1: Run fresh configure/build and non-window tests**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j2
ctest --test-dir build --exclude-regex 'window|backend-startup' --output-on-failure
ctest --test-dir build -R 'code-size' --output-on-failure
git diff --check HEAD~5..HEAD
```

Expected: all selected non-window tests pass; window behavior tests remain intentionally excluded.

- [x] **Step 2: Run sanitizer-focused non-window tests**

Configure a separate sanitizer build with `-DNEKOPLAYER_SANITIZERS=ON`, build the affected test targets, and run the appearance, playlist, transport, interaction, and quick-actions tests. Record compiler, Qt version, and exact CTest result in `docs/verification.md`.

- [x] **Step 3: Update handoff and explicitly list manual gaps**

Record the commits, token/QSS contract, QuickActions shortcut, and known gaps: real Wayland visual inspection at 100%/175% DPI, fullscreen/video pixel stability, compositor-only desktop blur, HDR output, hardware decode, and mini-player behavior. State that offscreen tests do not prove those paths.

- [x] **Step 4: Commit documentation**

```bash
git add docs/verification.md docs/appearance-handoff.md
git commit -m "docs: record apple ui convergence verification"
```

## Self-review checklist

- Spec coverage: A1 is Task 1, A2 is Task 2, A3 is Task 3, QuickActions is Task 4, and verification/Wayland acceptance is Task 5.
- Placeholder scan: no unresolved placeholder markers or unspecified "handle later" steps are present.
- Type consistency: `AppearanceTokens` fields are consumed by QSS and layout tasks; `QuickActionsDialog` constructor/accessors and MainWindow integration are defined in Task 4 and used by its tests.
- Scope: no playback engine, HDR, hardware-decoder, Vulkan, Qt Quick, plugin, cloud, or database work is introduced.
- User changes: `docs/skills.md` and existing uncommitted UI edits are not included until explicitly reviewed in the corresponding task.
