/**
  ******************************************************************************
  * @file    led.h
  * @brief   状态指示：PC13 心跳灯 + PB10 蜂鸣器
  *
  *          心跳灯含义（现场没接串口也能一眼看出车在什么状态）：
  *            常灭        : 停机
  *            1Hz 慢闪    : 循迹模式运行中
  *            4Hz 快闪    : 遥控模式
  *            双闪        : 故障（丢线/IMU 异常）
  *            常亮        : IMU 校准中 / 初始化中
  ******************************************************************************
  */

#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 心跳灯模式 */
typedef enum {
    LED_MODE_OFF = 0,       /* 常灭 */
    LED_MODE_TRACK,         /* 1Hz 慢闪 */
    LED_MODE_RC,            /* 4Hz 快闪 */
    LED_MODE_FAULT,         /* 双闪 */
    LED_MODE_BUSY           /* 常亮（校准/初始化） */
} LedMode_t;

/**
 * @brief  指示模块初始化
 */
void Led_Init(void);

/** 设置心跳灯模式 */
void Led_SetMode(LedMode_t mode);

/**
 * @brief  指示任务，必须按固定周期调用
 * @param  period_ms  调用周期 (ms)，通常传 CTRL_PERIOD_MS
 * @note   用软件计时而不是硬件定时器，省一个外设。
 */
void Led_Task(uint16_t period_ms);

/** 蜂鸣器 */
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint16_t ms);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H__ */
