/**
  ******************************************************************************
  * @file    main.h
  * @brief   公共头文件：只放标准库与 HAL 的引入、Error_Handler 声明。
  *          业务相关的东西一律放 bsp_config.h / 各模块自己的头文件。
  ******************************************************************************
  */

#ifndef __MAIN_H__
#define __MAIN_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 不直接包含 stm32f1xx_hal.h —— 由 bsp_config.h 的 MCU 移植层
 * 根据实际芯片选择 F1/F4 的 HAL 头文件。 */
#include "bsp_config.h"
#include <stdint.h>

/** 断言失败回调（定义在 main.c） */
void Error_Handler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H__ */
