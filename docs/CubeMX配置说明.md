# STM32CubeMX 配置说明

> 本工程的用法和"CubeMX 一键生成整个工程"**不同**，请先看完这一节，
> 否则会因为重复定义 / 外设被覆盖而编译失败。

## 一、分工原则

| 由 CubeMX 负责 | 由本工程代码负责 |
|---|---|
| 生成 HAL 库（`Drivers/`） | 时钟树配置（`Bsp_Clock_Config()`） |
| 生成启动文件（`Core/Startup/`） | 引脚复用与 GPIO 模式（`Bsp_Gpio_Init()`） |
| 生成链接脚本（`*.ld`） | PWM / 编码器 / 串口 / EXTI 初始化 |
| 生成 `stm32f1xx_hal_conf.h` | 全部业务逻辑 |

**为什么这样分**：CubeMX 把引脚写进 `main.c` 的 `MX_GPIO_Init()`，
一旦你重新生成就会覆盖。本工程把这些放到 `bsp_config.c`，
好处是重新生成 CubeMX 工程**不会破坏任何业务代码**，
而且以后换 MCU（F407VET6）只需要重写 `bsp_config.*`。

## 二、CubeMX 里要做的配置

### 1. 新建工程

- 选芯片：`STM32F103C8Tx`（LQFP48）
- 工程名随你，**Toolchain 选 `Makefile`**（与自带 Makefile 最匹配）
- 生成位置：让 `Core/` 和 `Drivers/` 直接落在工程根目录

### 2. Pinout 配置

只需要配**时钟和调试口**，其余引脚**不用在 CubeMX 里点**（由代码配）。

| 项目 | 设置 |
|---|---|
| `RCC` → High Speed Clock (HSE) | **Crystal/Ceramic Resonator** |
| `SYS` → Debug | **Serial Wire**（保留 SWD，否则烧不进第二次） |
| `SYS` → Timebase Source | **SysTick** |

> ⚠️ **不要**在 CubeMX 里给 USART2 / TIM1 / TIM2 / TIM3 勾选
> "Generate peripheral initialization as a pair of .c/.h files" 之外的东西，
> 也**不要**勾它们的 NVIC 中断，否则会与本工程的
> `USART2_IRQHandler` / `EXTI0_IRQHandler` 重复定义，链接报错。

### 3. Clock Configuration 页

虽然时钟由代码配置，但这一页要设成同样的值，让 CubeMX 校验合法性：

```
HSE             : 8 MHz
PLL Source      : HSE
PLL Mul         : ×9
SYSCLK          : 72 MHz
AHB Prescaler   : /1   -> 72 MHz
APB1 Prescaler  : /2   -> 36 MHz
APB2 Prescaler  : /1   -> 72 MHz
```

### 4. Project Manager 页

| 选项 | 设置 |
|---|---|
| Toolchain / IDE | Makefile |
| Code Generator → Copy only necessary library files | ✅ 勾上 |
| Code Generator → Generate peripheral initialization as pair of .c/.h | ❌ **不勾** |

## 三、生成后必须做的一件事

CubeMX 会在 `Core/Src/main.c` 里生成它自己的 `main()`，
并且可能在 `Core/Src/stm32f1xx_it.c` 里生成中断服务程序。

**处理办法**：生成后

1. **删除** `Core/Src/main.c`（本工程已有自己的 `main.c`），
   或把 CubeMX 生成的 `main.c` 改名为 `main_cubemx.c.bak` 后排除出编译。
   本工程的 `main.c` 已经包含了 `HAL_Init()` 和主循环。
2. 打开 `Core/Src/stm32f1xx_it.c`，**删掉**以下函数（如果存在）：
   - `SysTick_Handler()` —— 已在 `main.c` 实现
   - `EXTI0_IRQHandler()` —— 已在 `imu.c` 实现
   - `USART2_IRQHandler()` —— 已在 `comm.c` 实现

   保留其他的（如 `NMI_Handler`、`HardFault_Handler` 等）。
3. 确认 `Core/Inc/main.h` 也被替换成本工程的版本
   （CubeMX 生成的 `main.h` 里带一堆宏定义，本工程不需要）。

> 更省事的做法：CubeMX 生成完、把 `Drivers/` + `Core/Startup/` + `*.ld` +
> `stm32f1xx_hal_conf.h` 拷进本工程后，就**不再用 CubeMX 改引脚**了。
> 需要改引脚时直接改 `bsp_config.h`，比反复重新生成更可控。

## 四、Makefile 需要的文件清单

CubeMX 生成后，确认这些文件存在（否则 Makefile 会报错）：

```
Core/Startup/startup_stm32f103xb.s      启动文件
STM32F103C8Tx_FLASH.ld                  链接脚本（放在工程根目录）
Drivers/STM32F1xx_HAL_Driver/           HAL 库
Drivers/CMSIS/Device/ST/STM32F1xx/      CMSIS 设备头文件
Drivers/CMSIS/Include/                  CMSIS 内核头文件
Core/Inc/stm32f1xx_hal_conf.h           HAL 配置
```

链接脚本位置如果不在根目录，改 `Makefile` 里的 `LDSCRIPT` 变量即可。

## 五、`stm32f1xx_hal_conf.h` 需要打开的模块

CubeMX 生成的版本通常已经打开了用到的模块。确认以下几项是 `1`：

```c
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED      // PWM + 编码器
#define HAL_UART_MODULE_ENABLED     // 蓝牙
#define HAL_FLASH_MODULE_ENABLED
#define HAL_EXTI_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
```

同时确认这一行是打开的（`filter.c` 用到 `atan2f` / `sqrtf`）：

```c
#define USE_HAL_DRIVER
```

## 六、编译验证

```bash
make -j8
make size      # 看 Flash / RAM 占用
```

F103C8T6 的资源上限：

| 资源 | 容量 | 本工程预期占用 |
|---|---|---|
| Flash | 64 KB | 约 20~25 KB（HAL 库占大头） |
| RAM | 20 KB | 约 3~4 KB（环形缓冲 320B + 全局变量） |

如果 Flash 超了，优先把 `CFLAGS` 里的 `-O2` 提到 `-Os`。
