# SmartCar 技能配置

所选技能：

- ask-matt
- setup-matt-pocock-skills
- grill-me
- grill-with-docs
- grilling
- implement
- tdd
- diagnosing-bugs
- domain-modeling
- codebase-design
- improve-codebase-architecture
- code-review
- resolving-merge-conflicts
- wizard

用法：明确点名技能，例如“使用 grill-with-docs 澄清蓝牙协议需求”或“使用 diagnosing-bugs 排查编码器测速异常”。grill-with-docs 会调用 grilling 与 domain-modeling；implement 使用 tdd 与 code-review。架构改进先呈现候选方案，再按用户选择实施。

技能原始目录保留在 Cherry Studio 的 Data/Skills 下；本项目通过目录链接使用，原文件和配套资源保持完整。setup 已配置本地任务与单一领域上下文，未启用 triage。

wizard 上游使用 Bash；在 Windows 使用前确认 Bash 可用及向导所需命令。凭据由用户在可信界面输入；不写入项目文档。硬件步骤由用户确认操作结果后记录。

本项目可同时读取之前安装的个人级技能；此处清单是 SmartCar 重点工作流，不限制全局技能可用性。
