# STM32CubeMX 配置说明

> **一句话总结**：CubeMX 在本工程里只负责**生成 HAL 库 + 启动文件 + 链接脚本**，
> 以及**保留 SysTick 给 HAL 当时基**。所有引脚、外设、时钟都由
> `Core/Src/bsp_config.c` 在代码里配置。
>
> 好消息：按本文配完并生成后，**你不需要手改 CubeMX 生成的任何文件**
> （不用删 `main.c`，不用删 `SysTick_Handler`），只要把自己写的
> `bsp_config.c/main.c/...` 放进 `Core/` 就行。

---

## 一、为什么要这样分工

常见做法是"CubeMX 点好所有引脚 → 生成 → 在 USER CODE 里写业务"。
本工程反过来：**引脚和外设初始化全部手写在 `bsp_config.c`**。

| 由 CubeMX 负责 | 由本工程代码负责 |
|---|---|
| HAL 库（`Drivers/`） | 时钟树（`Bsp_Clock_Config()`） |
| 启动文件（`Core/Startup/`） | GPIO 模式与复用（`Bsp_Gpio_Init()`） |
| 链接脚本（`*.ld`） | TIM1 PWM / TIM2,TIM3 编码器 / TIM4 节拍 |
| `stm32f1xx_hal_conf.h` | USART2 / EXTI0 / 全部业务逻辑 |
| `SysTick_Handler`（HAL 时基） | —— |

这样做换来两个实际好处：

1. **重新生成 CubeMX 工程不会破坏业务代码**。CubeMX 改一次引脚就
   覆盖一次 `main.c` 的 `MX_GPIO_Init()`，是本工程要避开的坑。
2. **换 MCU 只改 `bsp_config.*`**。这个收益在你换 F407VET6 时会非常明显，
   见 `docs/移植到F407VET6.md`。

---

## 二、CubeMX 里要做的配置（完整清单）

### 步骤 1：新建工程

1. `File` → `New Project`，在 `Part Number` 里搜 `STM32F103C8`
2. 选 **`STM32F103C8Tx`**（LQFP48）→ `Start Project`

### 步骤 2：Pinout & Configuration 页

只需要动三个地方，**其他引脚一个都不要点**：

| 位置 | 设置 | 为什么 |
|---|---|---|
| `System Core` → `RCC` → High Speed Clock (HSE) | **Crystal/Ceramic Resonator** | 板上是 8MHz 晶振，必须选晶振而不是旁路 |
| `System Core` → `SYS` → Debug | **Serial Wire** | ⚠️ **最重要的一项**，见下方警告 |
| `System Core` → `SYS` → Timebase Source | **SysTick**（默认值，别改） | 留给 HAL 用 |

> ⚠️ **`Debug` 一定要选 `Serial Wire`**
> CubeMX 默认把 PA13/PA14 当普通 GPIO，一旦生成并烧进去，
> SWD 调试口就被关掉了，第二次就烧不进去（要靠拉 BOOT0 上电才能救回来）。
> 这是新手最常踩的坑，务必确认。

### 步骤 3：⚠️ 不要勾任何 NVIC 中断

这是本工程和常规用法**最大的区别**。在 `NVIC Settings` 里，
以下中断**全部保持不勾选**：

| 外设 | 不勾的原因 |
|---|---|
| `TIM1` | PWM 输出不需要中断（`bsp_config.c` 负责） |
| `TIM2` / `TIM3` | 编码器是纯硬件计数，不需要中断 |
| `TIM4` | **本工程的控制节拍中断**，ISR 在 `bsp_config.c` |
| `USART2` | ISR 在 `comm.c`（自己实现的环形缓冲） |
| `EXTI line0` | MPU6500 数据就绪中断，ISR 在 `imu.c` |

**勾了会怎样**：CubeMX 会在 `stm32f1xx_it.c` 里生成同名函数，
和你代码里的实现重复，链接时报
`multiple definition of 'TIM4_IRQHandler'`。

**唯一例外是 SysTick** —— 它是 HAL 的 1ms 时基（`HAL_Delay` 依赖它），
必须让 CubeMX 保留，本工程也**不再自己定义** `SysTick_Handler`。

### 步骤 4：Clock Configuration 页

虽然时钟由 `Bsp_Clock_Config()` 在代码里配，但这一页建议设成同样的值，
让 CubeMX 帮忙校验参数合法性：

```
Input frequency  : 8 MHz          (HSE)
PLL Source       : HSE
HSE / PLL Source : /1
PLL Mul          : ×9
System Clock Mux : PLLCLK
SYSCLK           : 72 MHz
AHB Prescaler    : /1   -> HCLK  72 MHz
APB1 Prescaler   : /2   -> 36 MHz   (定时器时钟 72MHz)
APB2 Prescaler   : /1   -> 72 MHz   (TIM1 时钟 72MHz)
```

页面上如果出现红色/黄色告警说明参数超规格，检查一下。

### 步骤 5：Project Manager 页

| 选项 | 设置 |
|---|---|
| `Project` → Toolchain / IDE | **Makefile**（与自带 `Makefile` 最匹配） |
| `Code Generator` → Copy only necessary library files | ✅ 勾上（减小体积） |
| `Code Generator` → Generate peripheral initialization as pair of .c/.h | ❌ 不勾 |
| `Code Generator` → Keep User Code when re-generating | ✅ 勾上 |

然后点 **GENERATE CODE**。

---

## 三、生成后怎么对接本工程

### 要保留的（从 CubeMX 生成目录拷进本工程）

```
Drivers/STM32F1xx_HAL_Driver/           HAL 库
Drivers/CMSIS/Device/ST/STM32F1xx/      CMSIS 设备头文件（含 stm32f1xx.h）
Drivers/CMSIS/Include/                  CMSIS 内核头文件
Core/Startup/startup_stm32f103xb.s      启动文件
Core/Src/stm32f1xx_it.c                 ⚠️ 必须保留，它提供 SysTick_Handler
Core/Src/system_stm32f1xx.c             系统初始化（含 SystemCoreClock）
Core/Inc/stm32f1xx_hal_conf.h           HAL 配置
STM32F103C8Tx_FLASH.ld                  链接脚本
```

> `stm32f1xx_it.c` **必须保留**：它里面的 `SysTick_Handler` 调用了
> `HAL_IncTick()`，是 `HAL_Delay` / `HAL_GetTick` 的基础。
> 本工程的控制节拍走 TIM4，所以**不需要**修改这个文件。

### 要被本工程替换的（用本工程的版本覆盖）

```
Core/Src/main.c        ← 本工程已包含 HAL_Init + Bsp_Init + App_Init + 主循环
Core/Inc/main.h        ← 本工程的版本只引入 bsp_config.h
```

CubeMX 生成的 `main.c` 里的 `MX_GPIO_Init()` / `MX_TIM1_Init()` 等
全部不需要 —— 功能由 `bsp_config.c` 完整提供。

> 更省事的做法：CubeMX 生成完、把上面"要保留的"拷进来之后，
> **就不要再动 CubeMX 了**。以后改引脚直接改 `bsp_config.h`，
> 比反复重新生成可控得多。

---

## 四、`stm32f1xx_hal_conf.h` 需要打开的模块

CubeMX 生成的版本通常已按你勾选的外设打开了对应模块。
如果编译报 `HAL_xxx_MODULE_ENABLED` 未定义，检查这几项：

```c
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED      /* PWM + 编码器 + TIM4 节拍 */
#define HAL_UART_MODULE_ENABLED     /* 蓝牙 JDY-31 */
#define HAL_FLASH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED     /* MPU6500 INT */
#define HAL_PWR_MODULE_ENABLED
```

同时确认（`filter.c` 用到 `atan2f` / `sqrtf`，虽由 `-lm` 提供，
但 HAL 也需要这个宏来选定头文件）：

```c
#define USE_HAL_DRIVER
```

---

## 五、编译验证

生成并拷好文件后：

```bash
make -j8          # 编译
make size         # 看 Flash / RAM 占用
make flash        # 用 ST-Link 烧录
```

如果没有 ST-Link，也可以用 STM32CubeProgrammer 的串口模式（BOOT0 拉高）烧录。

资源上限参考（F103C8T6）：

| 资源 | 容量 | 本工程预期占用 |
|---|---|---|
| Flash | 64 KB | 约 20~25 KB（HAL 库占大头） |
| RAM | 20 KB | 约 4 KB（环形缓冲 320B + 全局变量 + 栈） |

Flash 不够时把 `Makefile` 的 `OPT` 从 `-O2` 改成 `-Os`。

---

## 六、没生成 HAL 库之前想先检查代码？

本工程带了一个语法预检脚本，用 stub 头文件替代真实 HAL，
可以在还没装 CubeMX 的时候就把 `Core/Src` 全部检查一遍：

```bash
bash .ci_check/check.sh        # 检查 F1 和 F4 两个平台
bash .ci_check/check.sh F1     # 只检查 F103
```

它只能抓语法/类型/签名错误，**不能替代真机编译**。
新增了 HAL 外设（比如以后加 ADC）时，需要往
`.ci_check/stm32f1xx_hal.h` 里补对应的函数声明。

---

## 七、常见报错对照表

| 报错 | 原因 | 解决 |
|---|---|---|
| `multiple definition of 'SysTick_Handler'` | CubeMX 生成了，你也定义了 | 本工程不定义它，确认用的是本工程的 `main.c` |
| `multiple definition of 'TIM4_IRQHandler'` | CubeMX 里勾了 TIM4 的 NVIC | 取消勾选，重新生成；或删掉生成的那个函数 |
| `multiple definition of 'USART2_IRQHandler'` | CubeMX 里勾了 USART2 的 NVIC | 同上 |
| `undefined reference to 'HAL_TIM_Base_Init'` | `HAL_TIM_MODULE_ENABLED` 没开 | 打开该宏 |
| `undefined reference to 'HAL_GPIO_Init'` | `HAL_GPIO_MODULE_ENABLED` 没开 | 打开该宏 |
| `undefined reference to 'atan2f'` | 链接没带数学库 | `Makefile` 的 `LIBS` 里要有 `-lm` |
| 烧录一次后连不上 | CubeMX 没配 `Debug: Serial Wire` | 拉高 BOOT0 上电，重新烧一个配好的固件 |
| 电机完全不转 | TB6612 的 STBY 没拉高 | 检查 `MOTOR_STBY_PIN`(PB1) 接线，代码上电时会置高 |
| 蓝牙收不到数据 | JDY-31 出厂波特率是 9600 | 与 `COMM_BAUDRATE` 保持一致，或用 `AT+BAUD` 改模块 |
