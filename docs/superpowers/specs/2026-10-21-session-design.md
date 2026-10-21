# 会话与桌面偏好恢复

在已授权的持续开发范围内补齐正常退出、再次启动的日常体验，保持 C++20、Qt 6 Widgets、单一 libmpv 和原生 Wayland。普通手写代码文件 ≤500 行，现有已登记例外 ≤800 行。GUI 测试依用户要求只编译、不执行。

## 行为

- 正常关闭时保存队列路径、顺序、重复项、当前行、音量、静音、普通窗口 geometry（含最大化状态）、播放列表面板可见性、最后使用的媒体目录。
- 无位置参数启动时恢复队列和选择，但不加载媒体、不自动播放；播放键从当前行开始。缺失文件留在队列，真正播放时沿用现有跳过逻辑。
- 命令行文件替换恢复的队列。清空队列后的退出必须保存空队列，不能在下次启动复活。
- 全屏退出应用时保存进入全屏前的 geometry；不跨启动恢复全屏。Qt 的 geometry 恢复负责适配可用屏幕，不调用 X11 原生接口。
- `--no-config` 与 `--smoke-test` 都禁用会话读写；无 GUI 的 backend 诊断不访问配置。测试使用显式临时文件，不碰真实用户配置。
- 此阶段不保存播放进度、轨道选择或外挂字幕，不承诺崩溃恢复和多实例合并。

## 结构和性能

`src/session/sessionstate.h` 定义独立值类型：持久偏好与队列快照分开。`SessionStore` 仅依赖 Qt Core，以显式文件路径读写 schemaVersion=1 的 INI；默认路径由 main 使用 QStandardPaths::AppConfigLocation 决定。

`SessionManager` 连接 store、播放控制器和队列，通过业务 API 恢复音量、静音与空闲队列；采集当前快照供退出保存。恢复音频值的异步观察完成前，快照保留已载入的音频偏好，防止立即关闭把它们写成启动默认值。

`PlaylistController::restoreFiles` 替换队列和当前行但不播放；`PlaylistModel::paths` 返回队列快照。MainWindow 在所有控件装配后恢复会话，在渲染 ready 前禁用恢复队列的播放入口。窗口协调独立放入 `windowsession.cpp`，不把磁盘写入 time-pos 或帧回调。

只在启动读、正常关闭写。QSettings 要求原子同步；未来版本、损坏格式或不可写路径返回错误，不覆盖原始数据。独立无 GUI 测试使用真实 QSettings 和 headless libmpv，覆盖重启、错误和异步启动；窗口行为只编译并记录待验收边界。

## 验收

重建 SessionStore/SessionManager 后队列、当前行和偏好一致，播放器保持 idle；显式播放加载被选中的真实媒体。空队列覆盖旧队列；异常数值和无效当前行回落到可用值；不支持的 schema 和解析失败不会被保存覆盖。完整 Debug 构建、选定非 GUI CTest、ASan/UBSan 会话集成及 code-size、格式检查通过后提交。
