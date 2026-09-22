/**
  ******************************************************************************
  * @file    bsp_config.h
  * @brief   全车硬件资源集中定义 —— 唯一的"换板入口"
  *
  *          设计原则：
  *          1. 所有引脚 / 定时器 / 外设 / 控制周期常量都集中在本文件，
  *             驱动层（motor.c / encoder.c / imu.c ...）不出现任何裸的
  *             GPIOA / GPIO_PIN_x / htimX 字面量。
  *          2. 以后换更大容量的 STM32（如 F103RCT6 / F407 / G0 系列）时，
  *             只需要改本文件的引脚宏 + Bsp_Init()，业务代码零改动。
  *
  *          实际硬件（按采购清单）：
  *            MCU   : STM32F103C8T6 (LQFP48, 64K Flash / 20K RAM, 72MHz)
  *            电机  : MG520 减速电机 ×2，减速比 30，AB 相霍尔编码器 11 线
  *            驱动  : TB6612FNG 双路 H 桥（注意 STBY 必须拉高才工作）
  *            姿态  : MPU6500 模块（软件 SPI 读取）
  *            循迹  : 5 路 TCRT5000 数字量模块（检测黑线 = 低电平）
  *            通信  : JDY-31 蓝牙 SPP（默认波特率 9600）
  ******************************************************************************
  */

#ifndef __BSP_CONFIG_H__
#define __BSP_CONFIG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================================
 * 0. MCU 移植层
 *
 *    这里把 F1 / F4 之间**真正不同**的东西集中起来。
 *    驱动层（motor.c / encoder.c / imu.c / track.c / comm.c / control.c）
 *    使用的 HAL 接口在两个系列上是完全一致的：
 *        HAL_TIM_PWM_Init / HAL_TIM_Encoder_Init / HAL_UART_Init / HAL_GPIO_Init
 *    所以那些文件一个字都不用改。
 *
 *    换 F407VET6 的完整步骤见 docs/移植到F407VET6.md
 * ========================================================================== */

#if defined(STM32F103xB) || defined(STM32F103xE)
  /* ---------------- STM32F103C8T6 (当前) ---------------- */
  #include "stm32f1xx_hal.h"
  #define BSP_MCU_NAME          "STM32F103C8T6"
  #define BSP_SYSCLK_HZ         72000000U

  /* F1 的 GPIO 复用是靠 AFIO 重映射，HAL 的 GPIO_InitTypeDef 没有
   * Alternate 字段，所以这里的 AF 编号填了也不会被使用。 */
  #define BSP_AF_TIM1           0U
  #define BSP_AF_TIM2           0U
  #define BSP_AF_TIM3           0U
  #define BSP_AF_USART2         0U
  #define BSP_GPIO_HAS_AF       0

  /* F1 用 AFIO 管理 EXTI 线映射，F4 改用 SYSCFG */
  #define BSP_EXTI_CLK_ENABLE() __HAL_RCC_AFIO_CLK_ENABLE()

  /* F1 最高 72MHz，Flash 等待周期 2；F4 168MHz 需要 5 */
  #define BSP_FLASH_LATENCY     FLASH_LATENCY_2

  /* TIM1 挂在 APB2。F1 的 APB2 分频为 1 -> 定时器时钟 = 72MHz */
  #define BSP_TIM1_CLK_HZ       72000000U

  /* 控制节拍定时器 TIM4 挂在 APB1。
   * F1 的 APB1 分频为 2 (≠1)，所以定时器时钟 = 36 × 2 = 72MHz */
  #define BSP_TICK_TIM_CLK_HZ   72000000U

#elif defined(STM32F407xx)
  /* ---------------- STM32F407VET6 (后续升级目标) ---------------- */
  #include "stm32f4xx_hal.h"
  #define BSP_MCU_NAME          "STM32F407VET6"
  #define BSP_SYSCLK_HZ         168000000U

  /* F4 必须显式指定复用功能编号 */
  #define BSP_AF_TIM1           GPIO_AF1_TIM1
  #define BSP_AF_TIM2           GPIO_AF1_TIM2     /* PA0/PA1 */
  #define BSP_AF_TIM3           GPIO_AF2_TIM3     /* PA6/PA7 */
  #define BSP_AF_USART2         GPIO_AF7_USART2
  #define BSP_GPIO_HAS_AF       1

  #define BSP_EXTI_CLK_ENABLE() __HAL_RCC_SYSCFG_CLK_ENABLE()
  #define BSP_FLASH_LATENCY     FLASH_LATENCY_5

  /* TIM1 挂在 APB2。F4 的 APB2 分频为 2，定时器时钟 = APB2 × 2 = 168MHz。
   * 这里如果照抄 F1 的 72MHz，PWM 频率会变成 46.7kHz，
   * TB6612 的开关损耗会明显上升。 */
  #define BSP_TIM1_CLK_HZ       168000000U

  /* 控制节拍定时器 TIM4 挂在 APB1。
   * F4 的 APB1 分频为 4 (≠1)，所以定时器时钟 = 42 × 2 = 84MHz */
  #define BSP_TICK_TIM_CLK_HZ   84000000U

#else
  #error "bsp_config.h: 未支持的 MCU。请在此处添加对应系列的分支（参考上两条）。"
#endif

/* ==========================================================================
 * 1. 控制周期与采样窗口
 * ========================================================================== */

/** 主控制周期 (ms)。由 TIM4 定时中断置位控制标志，主循环消费 */
#define CTRL_PERIOD_MS          5U

/* --------------------------------------------------------------------------
 * 控制节拍定时器：TIM4
 *
 * 为什么不用 SysTick 做控制节拍：
 *   CubeMX 生成的 stm32f1xx_it.c 里**必定**包含 SysTick_Handler
 *   （用于 HAL_IncTick），如果本工程也在 main.c 里定义 SysTick_Handler，
 *   链接时就会重复定义；而且每次重新生成 CubeMX 工程都会再冲突一次。
 *
 *   改用独立硬件定时器 TIM4 后：
 *     - SysTick 完全交给 HAL，stm32f1xx_it.c 一个字都不用改
 *     - 控制节拍由硬件定时器产生，比"在 SysTick 里分频"更精确
 *     - 即使 HAL_Delay 阻塞（如 IMU 初始化）也不影响节拍的独立性
 *
 *   TIM4 时基分频到 10kHz，再计 10×CTRL_PERIOD_MS 个数得到控制周期：
 *     5ms -> 10kHz 下数 50 个 (ARR = 49)
 * -------------------------------------------------------------------------- */
#define BSP_TICK_TIM_PSC        ((BSP_TICK_TIM_CLK_HZ / 10000U) - 1U)
#define BSP_TICK_TIM_ARR        ((CTRL_PERIOD_MS * 10U) - 1U)
#define BSP_TICK_TIM_IRQn       TIM4_IRQn

/** 速度环采样窗口 = 多少个控制周期。
 *  MG520 输出轴一圈 1320 counts（11 线 × 30 减速比 × 4 倍频），
 *  额定 180rpm 时 5ms 只有约 20 个计数，量化误差 ±5%，速度环会抖。
 *  取 2 个周期（10ms）约 40 个计数，量化误差降到 ±2.5%，比较平衡。
 *  若要更平滑可加大到 4（20ms），代价是速度反馈滞后增加。 */
#define SPEED_WINDOW_TICKS      2U
#define SPEED_WINDOW_MS         (CTRL_PERIOD_MS * SPEED_WINDOW_TICKS)

/** 上电后陀螺仪零偏校准采样点数（静止标定，约 1~2 秒） */
#define IMU_CALIB_SAMPLES       500U

/* ==========================================================================
 * 2. 电机与编码器参数（MG520，减速比 30）
 * ========================================================================== */

#define MG520_ENCODER_LINES     11U     /* 编码器线数：电机每转 11 个方波 */
#define MG520_GEAR_RATIO        30U     /* 减速比（默认发货规格） */
#define ENCODER_MULTIPLIER      4U      /* 硬件正交编码器模式 4 倍频 */

/** 输出轴转一圈的计数值 = 11 × 30 × 4 = 1320 */
#define ENCODER_CPR             (MG520_ENCODER_LINES * MG520_GEAR_RATIO * ENCODER_MULTIPLIER)

/** MG520 减速比 30 的额定/空载转速 (rpm)，用于输出限幅与合理性检查 */
#define MOTOR_RATED_RPM         180
#define MOTOR_NOLOAD_RPM        320

/** 速度环内部单位换算：counts / SPEED_WINDOW_MS  ->  输出轴 rpm */
#define ENCODER_COUNTS_TO_RPM   ((float)(60000.0f / (ENCODER_CPR * SPEED_WINDOW_MS)))

/* ==========================================================================
 * 3. 引脚分配表（LQFP48）
 *
 *   功能                MCU 引脚   外设/模式
 *   ------------------  ---------  --------------------------------------
 *   HSE 晶振            PD0/PD1    OSC_IN / OSC_OUT (8MHz -> PLL×9 = 72MHz)
 *   SWD 调试            PA13/PA14  SWDIO / SWCLK（保留，勿占用）
 *   --------------------------------------------------- 定时器 & 驱动
 *   左电机 PWM          PA8        TIM1_CH1   复用推挽, 20kHz
 *   右电机 PWM          PA9        TIM1_CH2   复用推挽, 20kHz
 *   左电机方向 AIN1     PB12       GPIO 推挽输出
 *   左电机方向 AIN2     PB13       GPIO 推挽输出
 *   右电机方向 BIN1     PB14       GPIO 推挽输出
 *   右电机方向 BIN2     PB15       GPIO 推挽输出
 *   TB6612 使能 STBY    PB1        GPIO 推挽输出（高 = 芯片工作，必须！）
 *   --------------------------------------------------- 编码器
 *   左编码器 A/B        PA0 / PA1  TIM2_CH1/CH2  输入浮空, 正交编码器模式
 *   右编码器 A/B        PA6 / PA7  TIM3_CH1/CH2  输入浮空, 正交编码器模式
 *   --------------------------------------------------- 姿态 (软件 SPI)
 *   MPU6500 CS          PA4        GPIO 推挽输出（低有效）
 *   MPU6500 SCLK        PA5        GPIO 推挽输出（模式 3，空闲高）
 *   MPU6500 MISO        PA11       GPIO 上拉输入
 *   MPU6500 MOSI        PA12       GPIO 推挽输出
 *   MPU6500 INT         PB0        EXTI0 上升沿（数据就绪）
 *   --------------------------------------------------- 循迹 (5 路数字)
 *   循迹 OUT1(最左)     PB5        GPIO 上拉输入（黑线 = 低电平）
 *   循迹 OUT2           PB6        GPIO 上拉输入
 *   循迹 OUT3           PB7        GPIO 上拉输入
 *   循迹 OUT4           PB8        GPIO 上拉输入
 *   循迹 OUT5(最右)     PB9        GPIO 上拉输入
 *   --------------------------------------------------- 通信 & 人机
 *   蓝牙 TX -> MCU RX   PA3        USART2_RX  输入浮空
 *   蓝牙 RX <- MCU TX   PA2        USART2_TX  复用推挽
 *   蜂鸣器              PB10       GPIO 推挽输出（高 = 响）
 *   状态指示灯          PC13       GPIO 开漏输出（低 = 亮）
 * ========================================================================== */

/* ---- 3.1 电机 PWM：TIM1 CH1 / CH2 -------------------------------------- */
extern TIM_HandleTypeDef htim1;

#define MOTOR_PWM_HTIM          (&htim1)
#define MOTOR_PWM_CH_LEFT       TIM_CHANNEL_1
#define MOTOR_PWM_CH_RIGHT      TIM_CHANNEL_2
#define MOTOR_PWM_FREQ_HZ       20000U  /* 20kHz，超出人耳听觉上限，无啸叫 */

/* ---- 3.2 电机方向 + TB6612 STBY ---------------------------------------- */
#define MOTOR_AIN1_PORT         GPIOB
#define MOTOR_AIN1_PIN          GPIO_PIN_12
#define MOTOR_AIN2_PORT         GPIOB
#define MOTOR_AIN2_PIN          GPIO_PIN_13
#define MOTOR_BIN1_PORT         GPIOB
#define MOTOR_BIN1_PIN          GPIO_PIN_14
#define MOTOR_BIN2_PORT         GPIOB
#define MOTOR_BIN2_PIN          GPIO_PIN_15

/* TB6612 的 STBY：低电平整片关断（电机自由滑行），高电平工作。
 * 原设计文档漏掉此脚，不拉高则 PWM 和方向怎么给都不转。 */
#define MOTOR_STBY_PORT         GPIOB
#define MOTOR_STBY_PIN          GPIO_PIN_1
#define MOTOR_STBY_ACTIVE_HIGH  1

/* ---- 3.3 编码器：TIM2 / TIM3 硬件正交模式 ------------------------------ */
extern TIM_HandleTypeDef htim2;     /* 左轮 */
extern TIM_HandleTypeDef htim3;     /* 右轮 */

#define ENCODER_HTIM_LEFT       (&htim2)
#define ENCODER_HTIM_RIGHT      (&htim3)

/* ---- 3.4 MPU6500 软件 SPI ---------------------------------------------- */
#define IMU_CS_PORT             GPIOA
#define IMU_CS_PIN              GPIO_PIN_4
#define IMU_SCK_PORT            GPIOA
#define IMU_SCK_PIN             GPIO_PIN_5
#define IMU_MISO_PORT           GPIOA
#define IMU_MISO_PIN            GPIO_PIN_11
#define IMU_MOSI_PORT           GPIOA
#define IMU_MOSI_PIN            GPIO_PIN_12
#define IMU_INT_PORT            GPIOB
#define IMU_INT_PIN             GPIO_PIN_0
#define IMU_INT_EXTI_IRQn       EXTI0_IRQn

/* 软件 SPI 位延时（空操作循环次数）。
 * 72MHz 下每次循环约 3~4 个周期，4 次 ≈ 0.2μs -> SCLK ≈ 2.5MHz。
 * MPU6500 寄存器读写最高支持 1MHz，10MHz 为数据突发上限；
 * 这里刻意保守取 ~1MHz 量级，长杜邦线下更稳。 */
#define IMU_SPI_DELAY_LOOPS     2U

/* ---- 3.5 循迹：5 路数字量，黑线 = 低电平 ------------------------------- */
#define TRACK_CH_NUM            5U

#define TRACK1_PORT             GPIOB      /* OUT1 = 最左 */
#define TRACK1_PIN              GPIO_PIN_5
#define TRACK2_PORT             GPIOB
#define TRACK2_PIN              GPIO_PIN_6
#define TRACK3_PORT             GPIOB      /* OUT3 = 中间 */
#define TRACK3_PIN              GPIO_PIN_7
#define TRACK4_PORT             GPIOB
#define TRACK4_PIN              GPIO_PIN_8
#define TRACK5_PORT             GPIOB      /* OUT5 = 最右 */
#define TRACK5_PIN              GPIO_PIN_9

/** 检测到黑线时的电平。模块手册写"检测到黑线输出低电平"，
 *  但不同批次丝印/比较器接法可能相反，若实测循迹反向（车往离线方向跑），
 *  把这里改成 1 即可，不用改任何逻辑代码。 */
#define TRACK_ACTIVE_LEVEL      0U

/** 传感器位置权重（×100，对应 OUT1..OUT5，左负右正）。
 *  5 路间距 12~15mm，权重按等间距线性分布，用重心法算偏差。 */
#define TRACK_WEIGHT_1          (-200)
#define TRACK_WEIGHT_2          (-100)
#define TRACK_WEIGHT_3          (0)
#define TRACK_WEIGHT_4          (100)
#define TRACK_WEIGHT_5          (200)

/** 偏差方向符号：若发现循迹修正是反的（越修越偏），改 -1 即可。
 *  与 TRACK_ACTIVE_LEVEL 一起构成"不用改逻辑"的现场标定开关。 */
#define TRACK_DIR_SIGN          (+1)

/* ---- 3.6 通信：USART2 接 JDY-31 ---------------------------------------- */
extern UART_HandleTypeDef huart2;

#define COMM_HUART              (&huart2)
#define COMM_IRQn               USART2_IRQn

/** JDY-31 出厂默认波特率 9600（见模块手册 AT+BAUD 表）。
 *  原设计文档写 115200，与模块出厂值不符 —— 若要用 115200，
 *  先发 "AT+BAUD8\r\n"（末尾必须带 \r\n）永久改模块波特率，
 *  再把这里同步改成 115200。 */
#define COMM_BAUDRATE           9600U

/* ---- 3.7 指示灯 & 蜂鸣器 ----------------------------------------------- */
#define LED_PORT                GPIOC
#define LED_PIN                 GPIO_PIN_13
#define LED_ACTIVE_HIGH         0       /* PC13 板载 LED 为共阳接法，低电平点亮 */

#define BUZZER_PORT             GPIOB
#define BUZZER_PIN              GPIO_PIN_10
#define BUZZER_ACTIVE_HIGH      1

/* ==========================================================================
 * 4. 全局配置默认值（可由上位机通过 SET_PARAM 指令在线改）
 * ========================================================================== */
/* 循迹基础速度，单位 counts/速度窗口。
 * 换算：(counts × 60000) / (1320 × 10ms) = rpm
 *   20 counts -> 约 91 rpm   ← 推荐起调值，先在这个速度把 PID 调稳
 *   40 counts -> 约 181 rpm  ← 已是 MG520(1:30) 的额定转速，不建议直接上
 *   70 counts -> 约 318 rpm  ← 空载极限，实际会因负载掉速 */
#define CFG_BASE_SPEED_DEFAULT  20
#define CFG_TRACK_KP_DEFAULT    30      /* 循迹外环 PD 比例（1/100 标度） */
#define CFG_TRACK_KD_DEFAULT    120     /* 循迹外环 PD 微分（1/100 标度） */
#define CFG_SPEED_KP_DEFAULT    25      /* 速度内环 增量式 PID */
#define CFG_SPEED_KI_DEFAULT    12
#define CFG_SPEED_KD_DEFAULT    0
#define CFG_PWM_LIMIT_DEFAULT   1000    /* PWM 输出限幅（对应 100.0%） */

/** PWM 定时器 ARR 值，PWM 占空比用 0..MOTOR_PWM_LIMIT 表示 */
#define MOTOR_PWM_MAX           1000

/* ==========================================================================
 * 4.1 上电自动进入的模式
 *
 *   默认 0（停机）：上电后电机不动，必须靠蓝牙下发 SET_MODE 指令才启动。
 *   这是最终产品的正确行为。
 *
 *   但在还没有手机 App / 上位机之前，没法用蓝牙启动，车就完全动不了。
 *   做台架调试时把这里临时改成 1，上电校准完就会直接进循迹模式。
 *   调完记得改回 0。
 *
 *     0 = 停机（默认，安全）
 *     1 = 循迹
 *     2 = 遥控
 *     3 = 调试（只回传数据，电机不输出）
 *
 *   注意：用 autoStart 时上电那一刻车就会开始跑，
 *   所以下载完程序先别放地上，或者把轮子架起来。
 * ========================================================================== */
#define APP_AUTOSTART_MODE      0

/* ==========================================================================
 * 5. 初始化接口
 * ========================================================================== */

/**
 * @brief  时钟树配置：HSE 8MHz -> PLL×9 -> SYSCLK 72MHz
 *         AHB=72MHz, APB1=36MHz, APB2=72MHz
 */
void Bsp_Clock_Config(void);

/**
 * @brief  全车外设初始化（GPIO / TIM1 PWM / TIM2,TIM3 编码器 / USART2 / EXTI0）
 * @note   本函数自带完整的 GPIO 与外设配置，不依赖 CubeMX 生成的 MX_xxx_Init()。
 *         CubeMX 只用来生成 HAL 库、启动文件、链接脚本和 SystemClock 参考值。
 */
void Bsp_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_CONFIG_H__ */
