/*
 * 仅用于开发期的语法检查（stub），不属于固件。
 * 复用 F1 stub 的公共类型，再补上 F4 特有的部分，
 * 用来验证 bsp_config.c 里 #elif defined(STM32F407xx) 分支能否编过。
 */
#ifndef STUB_STM32F4XX_HAL_H
#define STUB_STM32F4XX_HAL_H

#include "stm32f1xx_hal.h"

/* ---- FPU 标志：真机上由 core_cm4.h 根据编译选项定义 ---- */
#define __FPU_PRESENT 1
#define __FPU_USED    1

/* ---- F4 的复用功能编号 ---- */
#define GPIO_AF1_TIM1   1U
#define GPIO_AF1_TIM2   1U
#define GPIO_AF2_TIM3   2U
#define GPIO_AF7_USART2 7U

/* ---- F4 的 Flash 等待周期 ---- */
#define FLASH_LATENCY_5 0x00000005U

/* ---- F4 的 PLLP 取值（stub 里只需要能被赋值） ---- */
#define RCC_PLLP_DIV2 0U

/* ---- F4 特有功能宏 ---- */
#define __HAL_RCC_SYSCFG_CLK_ENABLE()      do { } while (0)
#define __HAL_RCC_PWR_CLK_ENABLE()         do { } while (0)
#define __HAL_PWR_VOLTAGESCALING_CONFIG(x) do { (void)(x); } while (0)
#define PWR_REGULATOR_VOLTAGE_SCALE1       0x0000C000U

void HAL_PWREx_EnableOverDrive(void);

#endif /* STUB_STM32F4XX_HAL_H */
