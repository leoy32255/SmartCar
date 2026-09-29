# F407 修订3最小构建

Status: claimed

## Scope

用户请求：根据接线图与 spec 补齐真实 HAL/CMSIS、启动文件、链接脚本，解决时钟、定时器冲突与中断归属。
本次为保持停车的板级最小固件；完整控制/协议、传感器驱动与上位机不在本票内。

## Acceptance

- 固定官方依赖，提供可重复 Make 构建与 ELF/HEX/BIN/MAP。
- STM32F407VE：512 KiB Flash、128 KiB普通 SRAM，另有64 KiB CCM，不合并成连续192 KiB。
- 8 MHz HSE ->168 MHz；TIM1 PE9/PE11 2 kHz，TIM3/TIM4编码器，TIM6 5 ms节拍，SysTick HAL 1 ms。
- 修订3方向11、PWM0；SPI2 Mode0 /64，CS高；USART2 PD5/6；PB10 EXTI10；保留PA0/1、USB和SWD。
- 检查产物向量表与唯一中断归属，构建失败不得被隐藏。
- 无烧录、台架、电机运行或实车验证。

## Comments

- 2026-09-29：用户确认验证接口为“构建命令→ELF/HEX/BIN及向量表”和“板级初始化→时钟、定时器与停车引脚配置”；完整控制/协议测试随后续功能实现。
- 审查基线为本次开始时 HEAD `76239a8f9d14fa870b4087e9364c2e36c8bfce1d`。保留已有未跟踪文件，不把它们整体纳入提交。
