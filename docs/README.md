# NekoPlayer 文档索引

本目录区分**当前状态文档**、**一次性审计/交接存档**和**开发流程文档**，避免过期信息与现行说明混在一起。

## 当前状态

| 文档 | 内容 |
| --- | --- |
| [README](../README.md) | 产品概览、截图、快速开始与路线图（面向用户与维护者） |
| [features.md](features.md) | 逐项行为、边界与完整快捷键参考 |
| [architecture.md](architecture.md) | 分层边界、关键组件职责、渲染与持久化约束 |
| [development.md](development.md) | 开发环境、构建/测试预设、目录结构、代码约定与发布流程 |
| [verification.md](verification.md) | 按日期记录的构建、CTest、sanitizer 与真机验证结果 |
| [packaging.md](packaging.md) | Arch 打包、安装布局与 checksum 流程 |
| [references.md](references.md) | 上游参考项目、克隆提交与许可证核查边界 |
| [skills.md](skills.md) | 本机 Codex / Claude Code 开发 skills 的安装与使用 |
| [CHANGELOG](../CHANGELOG.md) | 版本变更与预发布说明 |
| [CONTRIBUTING](../CONTRIBUTING.md) | 参与开发、提交与测试要求 |
| [SECURITY](../SECURITY.md) | 漏洞报告范围与私密披露流程 |
| [assets/](assets/) | README 横幅与真实界面截图 |

## 审计与存档

| 文档 | 内容 |
| --- | --- |
| [audit/2026-10-28.md](audit/2026-10-28.md) | 2026-10-28 全面审计、整改记录与最终验证数据 |
| [history/2026-10-26-appearance-handoff.md](history/2026-10-26-appearance-handoff.md) | 外观收敛阶段交接存档（已过期，仅供追溯） |
| [superpowers/plans](superpowers/plans) | 各阶段实施计划（历史工作流记录） |
| [superpowers/specs](superpowers/specs) | 各阶段设计规格（历史工作流记录） |

## 维护约定

- 改变用户可见行为时，同步更新 `docs/features.md`；改变构建、测试或发布方式时，同步更新 `docs/development.md`；架构边界变化更新 `architecture.md`，验证结果更新 `verification.md`。不要只改代码。
- README 保持概览与视觉入口，详细行为下沉到 `features.md`，避免两处重复维护。
- 一次性交接、审计和阶段总结放入 `docs/history/` 或 `docs/audit/`，并在开头标注日期与是否过期；`superpowers/` 与 `history/` 中的文件路径按当时布局记录，不随目录重构回写。
- 文档中的测试数量、翻译数量等易变数据尽量不写死，改为指向 `verification.md` 或构建时输出。
