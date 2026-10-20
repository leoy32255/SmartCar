# SmartCar — STM32F407VET6 智能循迹小车

**从这里开始：[上手与实物联调流程](docs/上手与实物联调流程.md)。** 一份流程覆盖当前进度、排针与线材、工具准备、下载、面板、接线、模块检查和首次运行，不预设单片机经验。

截至2026-10-20：核心板型号已确认，尚未焊排针、未实施模块接线；新USB线待测。已有ST-Link V2、万用表、焊接工具；先走SWD裸板连接，DFU保留为另一条路线。真实下载、通信、电气及实车尚待验证。

当前软件包含XY-160D、双编码器、SPI2 MPU6500/MCP23S17、USART2协议、500ms控制心跳、故障锁存、速度/循迹控制与Windows本机面板。上电保持停车，运动必须人工启动；宽线/路口歧义停车，十字/T字识别与搜线尚未实现。

```sh
make -j8                  # 运动固件：build/F4/
make DIAGNOSTIC=1 -j8     # 停车诊断固件：build/F4-diagnostic/
make -j8 test             # 双目标、同源模拟器、主机/产物/依赖验证；不烧录
python -m panel.server    # 本机面板，默认SIM；先按主流程准备环境
```

[完整文档导航](docs/README.md) · [集中引脚表](docs/引脚分配.md) · [协议与控制参考](docs/F407协议与控制.md) · [历史记录](docs/archive/2026-10-20/README.md)

源码分层：`app`负责调度与故障传播，`control`负责状态与控制，`runtime`负责协议/命令/遥测，`uart`负责中断队列，`sensors`负责器件采样，`sensor_port`负责SPI事务，`motor/bsp`负责输出与板级资源。当前F407只编译`Core/F407/`，固定官方HAL/CMSIS依赖；旧`Core/Src/`属于历史F103，不混入当前构建。

软件测试、SIM交互、下载校验和实车验证分别记录。当前应用不提供USB虚拟串口，核心板Type-C不能直接充当面板COM；面板使用JDY-31蓝牙SPP或可选的3.3V逻辑USB转TTL。首次上板用诊断固件。
