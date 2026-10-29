# 开发 skills

已安装以下 skills；它们属于本机开发环境（Codex 与 Claude Code），不是播放器运行时依赖。

## Codex 环境

| 名称 | 来源 | 安装位置 |
| --- | --- | --- |
| cpp-pro | https://github.com/Jeffallan/claude-skills/tree/main/skills/cpp-pro | `~/.codex/skills/cpp-pro` |
| qt6-desktop | 本次编写：Qt 6 桌面工程、线程、对象所有权及 UI 验证 | `~/.codex/skills/qt6-desktop` |
| libmpv-integration | 本次编写：libmpv API、事件、渲染和关闭生命周期 | `~/.codex/skills/libmpv-integration` |
| Superpowers 6.3.0 | https://github.com/obra/superpowers ，Codex 官方插件市场发行 | Codex 插件缓存，由插件管理器维护 |

三个独立 skills 同时以符号链接登记在 `~/.agents/skills/`，便于当前 Codex 的技能发现。Superpowers 使用插件自己的 skills 发现机制，不额外复制一套。

Superpowers 安装命令：

```bash
codex plugin add superpowers@openai-api-curated --json
codex plugin list --json
```

已确认插件 `installed: true`、`enabled: true`；安装包版本为 6.3.0，缓存标识 `0722921d`。包含 14 个 skills，涵盖需求讨论、计划、TDD、调试、验证、代码审查和开发分支工作流。

独立 skills 下一轮可用；如果当前会话没有刷新技能列表，重启 Codex 后加载。可显式调用 `$cpp-pro`、`$qt6-desktop`、`$libmpv-integration`，Superpowers skills 按插件中的名称调用。

本项目当前已实现首个可播放版本。后续涉及 C++、Qt Widgets 或 libmpv 渲染生命周期时，分别使用对应的本地 skill；Superpowers 工作流按任务类型选择，而不是每次全量执行。

## Claude Code 环境

| 名称 | 来源 | 安装位置 |
| --- | --- | --- |
| cpp-pro | 同上（复用 Codex 安装） | `~/.claude/skills/cpp-pro`（符号链接） |
| qt6-desktop | 同上 | `~/.claude/skills/qt6-desktop`（符号链接） |
| libmpv-integration | 同上 | `~/.claude/skills/libmpv-integration`（符号链接） |
| qt-widgets-ui-design | 本次编写：Qt Widgets 视觉设计、设计令牌、玻璃材质、HiDPI、动效与可访问性 | `~/.claude/skills/qt-widgets-ui-design` |
| Superpowers 7.0.1 | https://github.com/obra/superpowers | `superpowers@superpowers-dev`，scope: user |

Claude Code 安装命令：

```bash
mkdir -p ~/.claude/skills
ln -sfn ~/.codex/skills/cpp-pro ~/.claude/skills/cpp-pro
ln -sfn ~/.codex/skills/qt6-desktop ~/.claude/skills/qt6-desktop
ln -sfn ~/.codex/skills/libmpv-integration ~/.claude/skills/libmpv-integration
claude plugin marketplace add obra/superpowers
claude plugin install superpowers@superpowers-dev
```

`~/.claude/skills/` 中的符号链接在会话加载时会被跟随（`claude plugin validate` 不跟随，会给出符号链接警告，属预期）。四个独立 skills 同时登记在 `~/.agents/skills/`。新增或插件安装后需重启 Claude Code 会话才会出现在技能列表；Superpowers 7.0.1 含 15 个 skills，按任务类型选择调用。Qt 视觉/主题/布局类任务用 `qt-widgets-ui-design`，Qt 工程机制用 `qt6-desktop`，两者配合而非互相替代。

