# SmartCar —— 基于 STM32F103C8T6 的智能循迹小车

串级 PID 双闭环循迹小车，四层软件架构，引脚/外设全部集中配置，
为后续升级到 **STM32F407VET6** 预留了低成本的移植路径。

---

## 一、实际硬件

| 模块 | 实际型号 | 关键参数 |
|---|---|---|
| 主控 | STM32F103C8T6 最小系统板 | LQFP48，64KB Flash / 20KB RAM，72MHz |
| 电机 ×2 | MG520 减速电机 | **减速比 30**，额定 12V，空载 320rpm / 额定 180rpm |
| 编码器 | AB 相增量式霍尔编码器 | **11 线**（电机每转 11 个方波），3.3V/5V |
| 电机驱动 | TB6612FNG | 双路 H 桥，逻辑 5V，**STBY 必须拉高** |
| 姿态 | MPU6500 模块 | 6 轴，IIC/SPI 双接口，DC3.5V |
| 循迹 | 5 路 TCRT5000 | 数字量，检测距离 10~15mm，**黑线 = 低电平** |
| 通信 | JDY-31 蓝牙 SPP | UART，**出厂默认 9600**，从机模式，唤醒密码 1234 |
| 电池 | 3S 锂电组 2800mAh | Type-C 充电 |
| 电源 | 3.3V/5V 多路电源模块 | 输入 6~24V，5V@3A，**3.3V@1A**，纹波 <50mV |

> **编码器分辨率**：`11 线 × 30 减速比 × 4 倍频 = 1320 counts / 输出轴一圈`
> 这个数字决定了速度环的采样窗口，见下文"控制参数"一节。

---

## 二、目录结构

```
SmartCar/
├── SmartCar.ioc                    CubeMX 工程（引脚/时钟源头）
├── Makefile                        构建脚本（支持 MCU_FAMILY=F1/F4）
├── STM32F103C8Tx_FLASH.ld          链接脚本（CubeMX 生成）
│
├── Core/
│   ├── Inc/
│   │   ├── main.h                  公共头文件
│   │   ├── bsp_config.h            ★ 全部引脚/外设/参数集中定义（换板只改这里）
│   │   ├── motor.h                 TB6612 驱动
│   │   ├── encoder.h               编码器测速
│   │   ├── imu.h                   MPU6500 软件 SPI
│   │   ├── track.h                 5 路循迹
│   │   ├── filter.h                互补滤波姿态解算
│   │   ├── pid.h                   通用 PID（位置式 + 增量式）
│   │   ├── control.h               串级 PID 运动控制
│   │   ├── comm.h                  蓝牙帧协议
│   │   ├── led.h                   心跳灯 + 蜂鸣器
│   │   └── app.h                   模式状态机 + 调度
│   ├── Src/                        与上面一一对应的 .c
│   └── Startup/                    启动文件（CubeMX 生成）
│
├── Drivers/                        HAL 库 + CMSIS（CubeMX 生成，勿手改）
├── build/                          编译输出（已 gitignore）
├── .vscode/                        VSCode 编译/烧录/调试配置
└── docs/
    ├── 引脚分配.md                 ★ 引脚总表 + 与原设计文档的差异说明
    ├── CubeMX配置说明.md           ★ CubeMX 该怎么配、哪些要删
    └── 移植到F407VET6.md           ★ 升级到 F407VET6 的完整步骤
```

---

## 三、软件架构

```
┌─────────────────────────────────────────────────────────┐
│  应用层    app.c      模式状态机 + 时间片调度            │
├─────────────────────────────────────────────────────────┤
│  控制层    control.c  串级 PID 调度                      │
│            pid.c      位置式 / 增量式 PID                │
├─────────────────────────────────────────────────────────┤
│  解算层    filter.c   互补滤波 -> Yaw / Pitch / Roll     │
├─────────────────────────────────────────────────────────┤
│  驱动层    motor / encoder / imu / track / comm / led    │
├─────────────────────────────────────────────────────────┤
│  板级      bsp_config.c  时钟 / GPIO / PWM / 编码器 / UART│
└─────────────────────────────────────────────────────────┘
```

### 控制结构（串级双闭环）

```
5 路循迹 ─> 加权重心偏差 ─> 外环 PD ─> 差速量 turn
                                          │
                            left  = base - turn
                            right = base + turn
                                          │
编码器 ──> 实际转速 ──> 内环增量式 PID ──> PWM ──> 电机
```

- **外环用 PD 不用 PID**：循迹误差不需要积分。车只要还在线上，静态偏差最终会被
  内环速度闭环消化掉；加了积分反而会在过弯时累积，出弯后产生反向过冲（画龙）。
- **内环用增量式 PID**：对输出限幅不敏感（限幅期间不会积累虚假输出），
  天然带积分项，能自动补偿左右电机摩擦力不一致导致的直行跑偏。

### 时间片调度（裸机）

```
SysTick 1ms (归 HAL)          TIM4 5ms (归本工程)
  └─ HAL_IncTick()              └─ App_Tick_ControlSet()
     └─ HAL_GetTick/Delay 用        └─ 置控制标志

main() while(1)
  ├─ Comm_Update()        每圈都跑（保证串口不丢字节）
  └─ if (控制标志)         严格 5ms
       ├─ Imu_ReadData + Filter_Update
       ├─ Track_ReadSensors
       ├─ Encoder_Update
       ├─ 按模式调度 Control_*
       └─ Led_Task
```

> 控制节拍用 **TIM4 而不是 SysTick**：CubeMX 生成的 `stm32f1xx_it.c` 里
> 必定包含 `SysTick_Handler`（HAL 的 1ms 时基）。本工程若再定义一次就会
> 链接冲突，而且每次重新生成 CubeMX 都会复发。改用 TIM4 后 SysTick 完全
> 归 HAL，CubeMX 生成的文件一个字都不用改。

> 没上 FreeRTOS 的原因：F103C8T6 只有 20KB RAM，RTOS 的任务栈 + 内核对象
> 要吃掉 3~4KB，而本项目只有"控制"和"通信"两个任务且周期固定，
> 时间片轮询完全够用，调试也更直观。换 F407VET6（192KB RAM）后再迁移。

---

## 四、快速开始

### 1. 准备 HAL 库

本工程的 `Drivers/`、`Core/Startup/`、`*.ld`、`stm32f1xx_hal_conf.h`
**需要由 CubeMX 生成**。详细步骤见 [docs/CubeMX配置说明.md](docs/CubeMX配置说明.md)。

关键点：CubeMX 只负责生成 HAL 库和启动文件，
**引脚和时钟由代码初始化**，这样重新生成 CubeMX 工程不会破坏业务代码。

### 2. 编译前的语法自检（可选，但推荐）

HAL 库要 CubeMX 生成后才存在，在那之前没法真正编译。
`.ci_check/` 下提供了一套 stub 头文件，可以先用 gcc 做纯语法/类型检查：

```bash
bash .ci_check/check.sh        # 同时检查 F103 与 F407
bash .ci_check/check.sh F1     # 只查 F103
```

它不产出任何目标文件，只抓拼写、类型、函数签名、括号这类错误。
本工程提交时两个平台都是 **`-Wall -Wextra` 零警告**通过。
新用了别的外设（ADC / DMA 等）时，需要往 stub 里补对应声明。

### 3. 编译与烧录

```bash
make -j8            # 编译
make size           # 查看 Flash / RAM 占用
make flash          # ST-Link 烧录（需 STM32CubeProgrammer CLI）
make flash-ocd      # 或 OpenOCD 烧录
```

VS Code 里按 `Ctrl+Shift+B` 直接编译，或用任务面板里的
`Build` / `Flash (OpenOCD)` / `Show sizes`。
F5 启动 OpenOCD 断点调试（需 Cortex-Debug 扩展）。

### 4. 上电后的现象

| 现象 | 含义 |
|---|---|
| 蜂鸣器短响一声 | 上电自检完成 |
| 心跳灯常亮约 2.5 秒 | IMU 零偏校准中（**此时车必须静止**） |
| 心跳灯熄灭 | 进入停机模式，等待指令 |
| 蜂鸣器短响 + 校准成功后 | 正常 |
| 蜂鸣器长响 600ms | **MPU6500 初始化失败**（查 SPI 接线 / CS / 供电） |
| 蜂鸣器响 200ms | 校准期间检测到车在动，请静止后重发校准指令 |
| 心跳灯双闪 + 鸣叫 | **循迹丢线超 1 秒**，已自动刹车 |

> 上电默认是**停机**状态，电机不会转。必须发 `SET_MODE` 才会动。

> **还没有手机 App 怎么办**：把 `bsp_config.h` 里的
> `APP_AUTOSTART_MODE` 临时改成 `1`，上电校准完就直接进循迹模式，
> 不用等上位机。调完记得改回 `0`。
> 注意用这个开关时上电就会开跑，先把轮子架起来再上电。

---

## 五、通信协议

蓝牙（USART2，默认 **9600** 8N1）上的帧格式：

```
┌──────┬──────┬──────┬─────┬─────────────┬──────────┐
│ 0xAA │ 0x55 │ CMD  │ LEN │  PAYLOAD    │ CHECKSUM │
└──────┴──────┴──────┴─────┴─────────────┴──────────┘
  帧头(2)        功能字  长度   LEN 字节      累加和

CHECKSUM = (CMD + LEN + ΣPAYLOAD) & 0xFF
```

多字节整数一律**小端**（低字节在前）。

### 上位机 → 小车

| CMD | 名称 | Payload | 说明 |
|---|---|---|---|
| `0x01` | SET_MODE | `[0]` 模式 | 0=停机 1=循迹 2=遥控 3=调试 |
| `0x02` | RC_DRIVE | `[0]` int8 左轮%<br>`[1]` int8 右轮% | 范围 -100~+100 |
| `0x03` | SET_PARAM | `[0]` 参数ID<br>`[1..2]` int16 值 | 在线调 PID |
| `0x04` | CALIB_IMU | 无 | 触发陀螺仪零偏校准 |
| `0x05` | SET_YAW_HOLD | `[0]` 使能<br>`[1..2]` int16 目标角×10 | 航向锁定 |

参数 ID：`0`=基础速度 `1`=循迹Kp `2`=循迹Kd `3`=速度Kp `4`=速度Ki `5`=速度Kd

### 小车 → 上位机

| CMD | 名称 | Payload | 说明 |
|---|---|---|---|
| `0x81` | TELEMETRY | 13 字节 | 见下，默认 100ms 一次 |
| `0x82` | ACK | `[0]` 被应答CMD<br>`[1]` 状态 | 0=成功 1=失败 |
| `0x83` | EVENT | `[0]` 事件码 | 见下 |

**TELEMETRY 13 字节**：

| 偏移 | 类型 | 内容 |
|---|---|---|
| 0 | uint8 | 循迹位图（bit0=最左 … bit4=最右） |
| 1 | int16 | 横向偏差（-200~+200） |
| 3 | int16 | 左轮速度（counts/窗口） |
| 5 | int16 | 右轮速度 |
| 7 | int16 | Yaw ×10（度） |
| 9 | int16 | Pitch ×10 |
| 11 | int16 | Roll ×10 |

**事件码**：`0x01` IMU初始化失败 `0x02` 校准成功 `0x03` 校准失败（车在动）
`0x04` 循迹丢线已停车

### 上位机示例（Python）

```python
import struct, serial, time

def build_frame(cmd, payload=b''):
    body = bytes([cmd, len(payload)]) + payload
    checksum = sum(body) & 0xFF
    return b'\xAA\x55' + body + bytes([checksum])

ser = serial.Serial('COM5', 9600, timeout=0.1)

# 切到循迹模式
ser.write(build_frame(0x01, bytes([1])))
time.sleep(0.1)

# 在线把基础速度调到 25
ser.write(build_frame(0x03, bytes([0]) + struct.pack('<h', 25)))

# 收遥测
buf = b''
while True:
    buf += ser.read(64)
    while len(buf) >= 5:
        i = buf.find(b'\xAA\x55')
        if i < 0:
            buf = buf[-1:]
            break
        if len(buf) < i + 4:
            break
        cmd, ln = buf[i+2], buf[i+3]
        if len(buf) < i + 5 + ln:
            break
        payload = buf[i+4:i+4+ln]
        if sum(buf[i+2:i+4+ln]) & 0xFF == buf[i+4+ln]:
            if cmd == 0x81:
                mask, dev, sl, sr, yaw, pitch, roll = struct.unpack('<Bhhhhhh', payload)
                print(f"mask={mask:05b} dev={dev:4d} L={sl:4d} R={sr:4d} yaw={yaw/10:.1f}")
        buf = buf[i+5+ln:]
```

---

## 六、控制参数与调参

### 速度单位说明

速度全部用 **counts / 速度窗口** 表示，不用 rpm —— 整数运算无精度损失，
PID 参数也更稳定。换算关系（窗口 10ms）：

| counts/窗口 | 输出轴 rpm | 说明 |
|---|---|---|
| 20 | 91 | `CFG_BASE_SPEED_DEFAULT`，推荐起调值 |
| 40 | 181 | MG520(1:30) 额定转速 |
| 70 | 318 | 空载极限，实际会因负载掉速 |

> 为什么速度窗口是 2 个控制周期（10ms）而不是 1 个（5ms）：
> 1320 counts/圈 下，额定转速时 5ms 内只有约 20 个计数，
> 量化误差 ±1 count 就是 ±5%，速度环会明显抖动。
> 取 10ms 后约 40 个计数，误差降到 ±2.5%，比较平衡。
> 想要更细腻可加大 `SPEED_WINDOW_TICKS` 到 4（20ms），代价是反馈滞后增加。

### 调参顺序（重要，别跳步）

**第 0 步：确认方向都对**

先不调 PID，手工验证四件事，任何一项反了后面都调不出来：

1. `Motor_SetPWM(300, 300)` → 两轮都**向前**转
   → 不对就改 `motor.h` 的 `MOTOR_LEFT_INVERT` / `MOTOR_RIGHT_INVERT`
2. 手向前推车 → `Encoder_GetLeftSpeed()` 与 `Encoder_GetRightSpeed()` **都为正**
   → 不对就改 `encoder.h` 的 `ENCODER_LEFT_INVERT` / `ENCODER_RIGHT_INVERT`
3. 把车放在黑线上，看 `Track_GetMask()` → 压线的那几路应该是 `1`
   → 全反了就改 `bsp_config.h` 的 `TRACK_ACTIVE_LEVEL`
4. 把线放在车**右侧** → `Track_GetDeviation()` 应为**正值**
   → 反了就改 `bsp_config.h` 的 `TRACK_DIR_SIGN`

**第 1 步：先调内环（速度环）**

把车架起来（轮子悬空），进入遥控模式，给一个固定目标速度：

1. `Ki = 0`、`Kd = 0`，从小往大加 `Kp`（`PARAM_SPEED_KP`）直到
   实际转速接近目标，且略有振荡
2. 加 `Ki`（`PARAM_SPEED_KI`）消除稳态误差 —— 加太多会低频摆动
3. `Kd` 一般保持 0，增量式 PID 的 Kd 对编码器噪声很敏感
4. **验证**：用手轻捏轮子增加负载，实际转速应该能回到目标值

推荐起点：`Kp=25, Ki=12, Kd=0`

**第 2 步：再调外环（循迹 PD）**

放到赛道上，基础速度先给低一点（`PARAM_BASE_SPEED` 从 15 开始）：

1. 只给 `Kp`（`PARAM_TRACK_KP`），从小往大加，
   直到车能跟着线走但开始左右轻微摆
2. 加 `Kd`（`PARAM_TRACK_KD`）提供阻尼，把摆动压下去
   → Kd 太大会对赛道噪点过敏，表现为车"抽搐"

推荐起点：`Kp=30, Kd=120`

**第 3 步：提速**

运行稳定后逐步提高 `PARAM_BASE_SPEED`，每提一次重新微调 Kp/Kd。
速度越高，Kd 需要越大（过弯的提前量需求变大）。

### 在线调参

不用反复烧录。用蓝牙发 `SET_PARAM` 帧即可实时改：

```python
ser.write(build_frame(0x03, bytes([1]) + struct.pack('<h', 40)))   # 循迹Kp=40
```

调好的值记得**回写到 `bsp_config.h` 的默认值**，
否则下次上电又变回去了。

---

## 七、几个容易踩的坑（都已在本工程中处理）

| 坑 | 现象 | 本工程的处理 |
|---|---|---|
| **TB6612 STBY 没拉高** | 接线全对但电机完全不转 | 给 STBY 单独分配 PB1，`Motor_Standby()` 控制 |
| **JDY-31 默认 9600** | 按 115200 通信全是乱码 | `COMM_BAUDRATE` 默认 9600 |
| **MPU6500 SPI 模式** | 软件 SPI 读不到数据 | 初始化时写 `USER_CTRL` 的 `I2C_IF_DIS` 位 |
| **MPU INT 脉冲太窄** | 采样率不稳、偶发丢帧 | 配置 `LATCH_INT_EN`，中断锁存到读 `INT_STATUS` |
| **循迹"全黑"有歧义** | 分不清压到横线还是被抬起来 | 区分 `LOST` / `ALL` / `SUSPECT` 三种状态 |
| **9600 波特率下阻塞发送** | 发遥测时循迹失控 | 收发都用中断 + 环形缓冲 |
| **16 位编码器计数器回绕** | 高速时速度突然变成 -65535 | `wrap_delta()` 做有符号回绕修正 |
| **增量式 PID 限幅爆冲** | 长时间堵转后突然猛冲 | 限幅后把输出写回累加器 |
| **Yaw 无绝对参考** | 航向角缓慢漂移 | 零偏校准 + 文档中明确限制；需要绝对航向须换磁力计 |
| **电机动力地干扰 MCU** | 跑着跑着复位 | 单点接地（硬件设计注意事项） |

---

## 八、升级到 STM32F407VET6

业务代码**一行都不用改**。需要动的只有 `bsp_config.*` 的 MCU 分支和构建参数：

```bash
make clean
make MCU_FAMILY=F4 -j8
```

F1/F4 之间真正不同的只有三件事，都已经隔离好了：

| 差异 | 处理方式 |
|---|---|
| 时钟树（72MHz → 168MHz） | `Bsp_Clock_Config()` 双分支，含过驱动与 FPU 使能 |
| GPIO 复用模型（AFIO → AF 编号） | `gpio_af_pp(pin, af)` + `BSP_AF_*` 宏 |
| EXTI 时钟（AFIO → SYSCFG） | `BSP_EXTI_CLK_ENABLE()` 宏 |

引脚几乎 1:1 平移（TIM1/TIM2/TIM3/USART2 的位置在两个系列上相同）。
完整步骤、三个最容易踩的坑、以及换板后值得做的升级
见 **[docs/移植到F407VET6.md](docs/移植到F407VET6.md)**。

---

## 九、已知限制

- **Yaw 会漂移**：MPU6500 没有磁力计，Yaw 只能靠陀螺 Z 轴积分，
  长时间必定漂移（约 1~5°/分钟）。只适合几秒~几十秒的短时航向锁定。
  需要绝对航向必须换带磁力计的 ICM-20948 / MPU9250。
- **软件 SPI 速率约 3MHz**：比硬件 SPI 慢，但 MPU6500 一次 14 字节读取
  只需约 40μs，占 5ms 周期的 0.8%，对本项目完全够用。
- **循迹只有 5 路**：分辨率有限，急弯需要降速通过。
  想提升可换 8 路模块（改 `TRACK_CH_NUM` 和引脚表即可，逻辑不用动）。
- **无电池电压监测**：3S 锂电过放会永久损坏。
  换 F407VET6 后建议加一路 ADC 分压监测，见移植文档第六节。
