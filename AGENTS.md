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
