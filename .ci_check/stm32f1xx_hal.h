/*
 * 仅用于开发期的语法检查（stub），不属于固件。
 * 目的是在还没生成 CubeMX HAL 库之前，就能用 gcc -fsyntax-only
 * 把 Core/Src 下的所有代码编译一遍，抓出拼写/类型/签名错误。
 * 真正的构建不包含这个目录（Makefile 的 include 路径里没有它）。
 */
#ifndef STUB_STM32F1XX_HAL_H
#define STUB_STM32F1XX_HAL_H

#include <stdint.h>
#include <stddef.h>

/* ---- 基础类型 ---- */
typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { RESET = 0, SET } FlagStatus;
typedef enum { DISABLE = 0, ENABLE } FunctionalState;

/* ---- GPIO ---- */
typedef struct { volatile uint32_t CRL, CRH, IDR, ODR, BSRR, BRR, LCKR; } GPIO_TypeDef;
/* 真实 HAL 里这些也是"常量地址表达式"形式的宏，而不是变量。
 * 这点很重要：它决定了 GPIO 句柄能不能出现在静态初始化列表里。 */
#define GPIOA ((GPIO_TypeDef *)0x40010800UL)
#define GPIOB ((GPIO_TypeDef *)0x40010C00UL)
#define GPIOC ((GPIO_TypeDef *)0x40011000UL)
#define GPIOD ((GPIO_TypeDef *)0x40011400UL)
#define GPIOE ((GPIO_TypeDef *)0x40011800UL)

#define GPIO_PIN_0  ((uint16_t)0x0001)
#define GPIO_PIN_1  ((uint16_t)0x0002)
#define GPIO_PIN_2  ((uint16_t)0x0004)
#define GPIO_PIN_3  ((uint16_t)0x0008)
#define GPIO_PIN_4  ((uint16_t)0x0010)
#define GPIO_PIN_5  ((uint16_t)0x0020)
#define GPIO_PIN_6  ((uint16_t)0x0040)
#define GPIO_PIN_7  ((uint16_t)0x0080)
#define GPIO_PIN_8  ((uint16_t)0x0100)
#define GPIO_PIN_9  ((uint16_t)0x0200)
#define GPIO_PIN_10 ((uint16_t)0x0400)
#define GPIO_PIN_11 ((uint16_t)0x0800)
#define GPIO_PIN_12 ((uint16_t)0x1000)
#define GPIO_PIN_13 ((uint16_t)0x2000)
#define GPIO_PIN_14 ((uint16_t)0x4000)
#define GPIO_PIN_15 ((uint16_t)0x8000)

#define GPIO_PIN_RESET 0
#define GPIO_PIN_SET   1

typedef enum {
    GPIO_MODE_INPUT = 0, GPIO_MODE_OUTPUT_PP, GPIO_MODE_OUTPUT_OD,
    GPIO_MODE_AF_PP, GPIO_MODE_AF_OD, GPIO_MODE_ANALOG,
    GPIO_MODE_IT_RISING, GPIO_MODE_IT_FALLING, GPIO_MODE_IT_RISING_FALLING
} GPIO_Mode_TypeDef;

typedef enum { GPIO_NOPULL = 0, GPIO_PULLUP, GPIO_PULLDOWN } GPIOPuPd_TypeDef;
typedef enum {
    GPIO_SPEED_FREQ_LOW = 0, GPIO_SPEED_FREQ_MEDIUM,
    GPIO_SPEED_FREQ_HIGH, GPIO_SPEED_FREQ_VERY_HIGH
} GPIOSpeed_TypeDef;

typedef struct {
    uint32_t Pin; uint32_t Mode; uint32_t Pull; uint32_t Speed;
    uint32_t Alternate;     /* F1 无此字段，F4 有；放在这里让两边都能编过 */
} GPIO_InitTypeDef;

void HAL_GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g);
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, uint32_t st);
uint32_t HAL_GPIO_ReadPin(GPIO_TypeDef *p, uint16_t pin);

/* ---- TIM ---- */
typedef struct { volatile uint32_t CR1, CR2, SMCR, DIER, SR, EGR, CCMR1, CCMR2, CCER, CNT, PSC, ARR; } TIM_TypeDef;
#define TIM1 ((TIM_TypeDef *)0x40012C00UL)
#define TIM2 ((TIM_TypeDef *)0x40000000UL)
#define TIM3 ((TIM_TypeDef *)0x40000400UL)
#define TIM4 ((TIM_TypeDef *)0x40000800UL)

typedef struct {
    uint32_t Prescaler, CounterMode, Period, ClockDivision,
             RepetitionCounter, AutoReloadPreload;
} TIM_Base_InitTypeDef;

typedef struct { TIM_TypeDef *Instance; TIM_Base_InitTypeDef Init; } TIM_HandleTypeDef;

typedef struct {
    uint32_t OCMode, Pulse, OCPolarity, OCFastMode;
} TIM_OC_InitTypeDef;

typedef struct {
    uint32_t EncoderMode;
    uint32_t IC1Polarity, IC1Selection, IC1Prescaler, IC1Filter;
    uint32_t IC2Polarity, IC2Selection, IC2Prescaler, IC2Filter;
} TIM_Encoder_InitTypeDef;

#define TIM_CHANNEL_1 0x00000000U
#define TIM_CHANNEL_2 0x00000004U
#define TIM_CHANNEL_ALL 0x0000003CU
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_OCMODE_PWM1 0x00000060U
#define TIM_OCPOLARITY_HIGH 0U
#define TIM_OCFAST_DISABLE 0U
#define TIM_ICPOLARITY_RISING 0U
#define TIM_ICSELECTION_DIRECTTI 1U
#define TIM_ICPSC_DIV1 0U
#define TIM_ENCODERMODE_TI12 0x00000003U

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *h);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *h, TIM_OC_InitTypeDef *c, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *h, uint32_t ch);
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *h, TIM_Encoder_InitTypeDef *e);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *h, uint32_t ch);

/* ---- UART ---- */
typedef struct { volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR; } USART_TypeDef;
#define USART1 ((USART_TypeDef *)0x40013800UL)
#define USART2 ((USART_TypeDef *)0x40004400UL)
#define USART3 ((USART_TypeDef *)0x40004800UL)

typedef struct {
    uint32_t BaudRate, WordLength, StopBits, Parity, Mode, HwFlowCtl, OverSampling;
} UART_InitTypeDef;

typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;

#define UART_WORDLENGTH_8B 0U
#define UART_STOPBITS_1 0U
#define UART_PARITY_NONE 0U
#define UART_MODE_TX_RX 0x0000000CU
#define UART_HWCONTROL_NONE 0U
#define UART_OVERSAMPLING_16 0U

#define USART_SR_RXNE 0x0020U
#define USART_SR_ORE  0x0008U
#define USART_SR_TXE  0x0080U
#define USART_CR1_RXNEIE 0x0020U
#define USART_CR1_TXEIE  0x0080U

HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *h);

/* ---- RCC ---- */
/* 一个结构体同时容纳 F1 的 PLLMUL 和 F4 的 PLLM/N/P/Q，
 * 这样两边都不用改类型定义（真机上它们确实是两个不同的结构体，
 * 但本 stub 只做语法检查，多几个字段无妨）。 */
typedef struct {
    uint32_t PLLState, PLLSource;
    uint32_t PLLMUL;                    /* F1 */
    uint32_t PLLM, PLLN, PLLP, PLLQ;    /* F4 */
} RCC_PLLInitTypeDef;
typedef struct {
    uint32_t OscillatorType, HSEState, HSEPredivValue, HSIState;
    RCC_PLLInitTypeDef PLL;
} RCC_OscInitTypeDef;
typedef struct {
    uint32_t ClockType, SYSCLKSource, AHBCLKDivider, APB1CLKDivider, APB2CLKDivider;
} RCC_ClkInitTypeDef;

#define RCC_OSCILLATORTYPE_HSE 0x00000001U
#define RCC_HSE_ON 0x00010000U
#define RCC_HSE_PREDIV_DIV1 0U
#define RCC_HSI_ON 0x00000001U
#define RCC_PLL_ON 0x00000002U
#define RCC_PLLSOURCE_HSE 0x00010000U
#define RCC_PLL_MUL9 0x001C0000U
#define RCC_CLOCKTYPE_HCLK 0x00000002U
#define RCC_CLOCKTYPE_SYSCLK 0x00000001U
#define RCC_CLOCKTYPE_PCLK1 0x00000004U
#define RCC_CLOCKTYPE_PCLK2 0x00000008U
#define RCC_SYSCLKSOURCE_PLLCLK 0x00000002U
#define RCC_SYSCLK_DIV1 0U
#define RCC_HCLK_DIV1 0U
#define RCC_HCLK_DIV2 0x00000400U
#define RCC_HCLK_DIV4 0x00000500U
#define RCC_HCLK_DIV8 0x00000600U
#define RCC_HCLK_DIV16 0x00000700U
#define FLASH_LATENCY_2 0x00000002U

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *o);
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *c, uint32_t lat);

/* ---- NVIC / IRQ ---- */
typedef enum { NonMaskableInt_IRQn = -14, EXTI0_IRQn = 6, USART2_IRQn = 38 } IRQn_Type;
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t pre, uint32_t sub);
void HAL_NVIC_EnableIRQ(IRQn_Type irq);

/* ---- 内核 ---- */
void HAL_Init(void);
void HAL_Delay(uint32_t ms);
void HAL_IncTick(void);
uint32_t HAL_GetTick(void);
uint32_t HAL_SYSTICK_Config(uint32_t ticks);
extern uint32_t SystemCoreClock;

#define __NOP() do { } while (0)
#define __disable_irq() do { } while (0)
#define __enable_irq()  do { } while (0)

/* CPACR 是 F4 使能 FPU 用的（F1 上该寄存器存在但不用） */
typedef struct {
    volatile uint32_t CPUID, ICSR, VTOR, AIRCR, SCR, CCR, SHPR[3],
                      SHCSR, CFSR, HFSR, DFSR, MMFAR, BFAR, AFSR, CPACR;
} SCB_Type;
extern SCB_Type *SCB;

/* ---- 时钟使能宏 ---- */
#define __HAL_RCC_GPIOA_CLK_ENABLE() do { } while (0)
#define __HAL_RCC_GPIOB_CLK_ENABLE() do { } while (0)
#define __HAL_RCC_GPIOC_CLK_ENABLE() do { } while (0)
#define __HAL_RCC_AFIO_CLK_ENABLE()  do { } while (0)
#define __HAL_RCC_TIM1_CLK_ENABLE()  do { } while (0)
#define __HAL_RCC_TIM2_CLK_ENABLE()  do { } while (0)
#define __HAL_RCC_TIM3_CLK_ENABLE()  do { } while (0)
#define __HAL_RCC_USART2_CLK_ENABLE() do { } while (0)

/* ---- 外设寄存器访问宏 ---- */
#define __HAL_TIM_SET_COMPARE(h, ch, v)   do { (void)(h); (void)(ch); (void)(v); } while (0)
#define __HAL_TIM_SET_COUNTER(h, v)       do { (void)(h); (void)(v); } while (0)
#define __HAL_TIM_GET_COUNTER(h)          ((uint32_t)0)

#define __HAL_GPIO_EXTI_GET_IT(pin)   (0U)
#define __HAL_GPIO_EXTI_CLEAR_IT(pin) do { (void)(pin); } while (0)

#define __HAL_UART_ENABLE_IT(h, it)  do { (void)(h); (void)(it); } while (0)
#define UART_IT_RXNE 0x0020U

#endif /* STUB_STM32F1XX_HAL_H */
