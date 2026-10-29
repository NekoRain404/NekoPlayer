# 参与开发

感谢你愿意改进 NekoPlayer。项目以 Linux 桌面为首要目标，使用 C++20、Qt 6 Widgets 与 libmpv。

## 开始之前

1. 阅读 [开发指南](docs/development.md) 的依赖、构建与目录结构。
2. 阅读 [架构说明](docs/architecture.md)，确认改动落在正确的层：UI 不直接发原始 mpv 命令，播放状态、图形资源与桌面交互分离。
3. 用户可见行为变化同步更新 [功能参考](docs/features.md)；测试证据更新到 [验证记录](docs/verification.md)。

## 本地检查

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

- 生命周期、回调或关闭顺序相关改动：再跑 `cmake --preset sanitizers` 与聚焦测试。
- 窗口行为：使用 `debug-x11`（Xvfb + Openbox）或原生 Wayland 桌面预设。
- 保持普通文件 ≤500 行（自动检查容忍到 550；`cmake/CheckCodeSize.cmake` 的明确例外可到 800），`-Wall -Wextra -Wpedantic` 0 警告。
- 使用 `clang-format`；不要为压行数压缩格式。

## 提交与 Pull Request

- 提交信息使用 Conventional Commits 前缀：`feat:`, `fix:`, `perf:`, `docs:`, `i18n:`, `chore:`, `build:`, `security:`。
- 一个提交聚焦一个主题；避免把重构、行为和文档混在一起。
- PR 描述写清动机、行为变化、验证命令与已知边界；界面变化附截图。
- 不提交构建产物、`references/` 克隆或本地配置；不要伪造测试结果。
- 安全问题不要开公开 issue，见 [SECURITY.md](SECURITY.md)。

## 许可证

贡献代码按 GPL-3.0-only 授权。引用上游实现前核对许可证，并在 PR 中说明来源。
