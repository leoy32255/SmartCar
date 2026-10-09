# SmartCar 工作约定

本项目是 STM32 智能循迹小车固件。改动前阅读 README.md 与相关 docs 文档，以当前源码、配置和用户确认的硬件为准。

## Agent skills

- 项目所选技能与适配方式见 `docs/agents/skills.md`；技能入口在 `.agents/skills/`。
- 规划与实现任务使用本地 Markdown，见 `docs/agents/issue-tracker.md`。
- 术语与架构决策采用单一上下文，见 `docs/agents/domain.md`。

## 实现与验证

- 引脚、外设与控制参数改动先核对 `Core/Inc/bsp_config.h` 和 `docs/引脚分配.md`，保留用户确认的接线与硬件约束。
- 尊重 README 描述的模块边界。修改 CubeMX 生成文件时保留 USER CODE 区；HAL/CMSIS 依赖通过生成或依赖管理维护。
- 算法和协议的可运行逻辑使用有意义的主机测试。涉及固件时检查 Makefile 的当前目标、依赖与构建产物。
- `.ci_check/check.sh` 使用 HAL 桩，只验证语法与类型；其找不到编译器时会跳过，须检查输出，不能仅凭退出码声称验证通过。
- 分别报告主机测试、交叉编译、烧录和实车验证。静态检查或构建成功不能证明电机、传感器、时序及闭环控制的真实表现。
- 本次配置不触发烧录或电机运行。提交仅包含当前任务变更，保留无关修改；推送按用户授权执行。

## 临时脚本与复用

- `tmp/` 中的一次性脚本完成任务、核对结果后立即删除；失败后仍需用于排查的脚本可暂留，排查结束后清理。
- 创建脚本前先检查本文件及 `tools/`、`tests/` 中的现有入口；已有相同功能时直接复用，不重复创建。
- 可复用脚本放在 `tools/`（测试放在 `tests/`），并在本文件登记用途、运行命令、参数、依赖及写入范围；不要只留在 `tmp/`。
- 清理只针对本次任务的一次性脚本；历史源码验证副本、第三方依赖中的脚本及原始资料须先核对用途，不按文件扩展名批量删除。

现有可复用入口（均在仓库根目录运行）：

| 用途 | 命令 | 依赖与作用范围 |
|---|---|---|
| 清理两种 F407 构建输出 | `python tools/clean_f407.py`（或 `make clean`） | Python；删除 `build/F4/` 与 `build/F4-diagnostic/`，拒绝重定向目录。 |
| 恢复固定版本的官方依赖 | `python tools/vendor_f407.py <HAL checkout> <device checkout> <CMSIS_5 checkout>` | Python、Git及三个干净的上游检出目录；校验脚本内固定提交后，写入 `Drivers/`、`Core/Startup/startup_stm32f407xx.s` 和 `Drivers/manifest.json`。仅在明确恢复或更新依赖时运行。 |
| 完整软件验证 | `make test` | ARM GCC、GNU Make、Python、主机 GCC；构建双固件目标与模拟器，运行主机、产物和依赖测试。具体配置见 `docs/F407最小构建.md`；不触发烧录。 |
