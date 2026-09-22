/**
  ******************************************************************************
  * @file    motor.h
  * @brief   TB6612FNG 双路电机驱动接口
  *
  *          正转定义以"车前进"为参考：左轮正转 = 车前进，右轮正转 = 车前进。
  *          若实测某侧方向相反（电机线序/齿轮箱朝向不同），
  *          改 MOTOR_LEFT_INVERT / MOTOR_RIGHT_INVERT 即可，不用动逻辑。
  ******************************************************************************
  */

#ifndef __MOTOR_H__
#define __MOTOR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 硬件方向修正开关：1 = 该轮正转方向取反 */
#define MOTOR_LEFT_INVERT       0
#define MOTOR_RIGHT_INVERT      0

/**
 * @brief  电机驱动初始化（GPIO 已在 Bsp_Init 配好，这里只做状态复位）
 */
void Motor_Init(void);

/**
 * @brief  设置两轮 PWM 占空比
 * @param  left   左轮，范围 -MOTOR_PWM_MAX ~ +MOTOR_PWM_MAX，带符号（负 = 反转）
 * @param  right  右轮，同上
 * @note   自动处理方向引脚；传 0 时该轮进入刹车（两端短接）状态。
 */
void Motor_SetPWM(int16_t left, int16_t right);

/**
 * @brief  只设置方向，不改 PWM（调试用）
 * @param  left_dir   0=滑行, 1=正转, -1=反转, 2=刹车
 * @param  right_dir  同上
 */
void Motor_SetDir(int8_t left_dir, int8_t right_dir);

/**
 * @brief  刹车：H 桥两端短接，电机快速停止（PWM 归零）
 */
void Motor_Brake(void);

/**
 * @brief  滑行：H 桥关断，电机自由减速（PWM 归零）
 */
void Motor_Coast(void);

/**
 * @brief  TB6612 STBY 使能控制
 * @param  enable  1 = 芯片工作，0 = 整片关断（进入最低功耗，电机滑行）
 * @note   上电必须调用 Motor_Standby(1) 电机才会转，这是 TB6612 的硬性要求。
 */
void Motor_Standby(uint8_t enable);

/**
 * @brief  读取当前两轮 PWM 设定值（供遥测回传）
 */
void Motor_GetPWM(int16_t *left, int16_t *right);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_H__ */
