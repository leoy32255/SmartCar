/**
  ******************************************************************************
  * @file    control.h
  * @brief   运动控制层：串级 PID 调度
  *
  *          控制结构（外环慢、内环快，符合串级控制原则）：
  *
  *            5 路循迹 ──> 偏差计算 ──> 外环 PD ──> 差速量 turn
  *                                                      │
  *                                        left  = base - turn
  *                                        right = base + turn
  *                                                      │
  *            编码器 ────> 实际转速 ──> 内环增量式 PID ──> PWM ──> 电机
  *
  *          为什么外环用 PD 而不是 PID：
  *            循迹误差不需要积分 —— 只要车还在线上，静态偏差最终会被
  *            内环速度闭环消化掉。加积分反而会在过弯时累积，
  *            出弯后产生明显的反向过冲（"画龙"）。
  *
  *          为什么内环用增量式 PID：
  *            增量式对输出限幅不敏感（限幅时不会积累虚假输出），
  *            天然带积分项，能自动补偿左右电机摩擦力不一致导致的直行跑偏。
  ******************************************************************************
  */

#ifndef __CONTROL_H__
#define __CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 参数 ID（供上位机在线调参用，见 comm.c） */
typedef enum {
    PARAM_BASE_SPEED = 0,
    PARAM_TRACK_KP,
    PARAM_TRACK_KD,
    PARAM_SPEED_KP,
    PARAM_SPEED_KI,
    PARAM_SPEED_KD,
    PARAM_COUNT
} Control_ParamId_t;

/**
 * @brief  控制层初始化（PID 参数取 bsp_config.h 里的默认值）
 */
void Control_Init(void);

/**
 * @brief  控制周期调度入口
 * @note   必须在每个控制周期（CTRL_PERIOD_MS）调用一次，
 *         调用前应已完成 Encoder_Update()、Track_ReadSensors()、Imu/Filter 更新。
 */
void Control_Update(void);

/**
 * @brief  切换到"直控"模式：跳过循迹外环，直接给定两轮目标速度
 * @param  enable  1 = 直控（遥控模式用），0 = 恢复循迹串级
 * @note   遥控模式下上位机给的是左右轮速度百分比，不需要循迹外环参与，
 *         但内环速度 PID 仍然要跑 —— 这样才有"给定速度 -> 实际速度"的闭环，
 *         否则电池电压下降时车会越跑越慢。
 */
void Control_SetDirectMode(uint8_t enable);

/**
 * @brief  设置直控模式下的两轮目标速度（counts / 速度窗口）
 */
void Control_SetDirectTarget(int16_t left, int16_t right);

/**
 * @brief  立即停车：PWM 归零 + 电机滑行 + 复位 PID 状态
 * @note   复位 PID 很重要，否则下次启动时增量式 PID 会带着上次的输出值起步。
 */
void Control_Stop(void);

/**
 * @brief  刹车停车（H 桥短接，比滑行停得快）
 */
void Control_Brake(void);

/** 设置循迹基础速度（counts / 速度窗口） */
void Control_SetBaseSpeed(int16_t speed);
int16_t Control_GetBaseSpeed(void);

/** 设置/读取指定参数，返回 0 成功、1 参数非法 */
uint8_t Control_SetParam(Control_ParamId_t id, int16_t value);
int16_t Control_GetParam(Control_ParamId_t id);

/** 调试/遥测用：本轮的目标速度与实际速度 */
void Control_GetTargetSpeed(int16_t *left, int16_t *right);
void Control_GetActualSpeed(int16_t *left, int16_t *right);

/** 当前外环输出的差速量，正值表示正在向右修 */
int16_t Control_GetTurn(void);

/**
 * @brief  航向锁定修正：用 IMU 的 Yaw 误差叠加到差速量上
 * @param  enable       1 = 开启
 * @param  target_yaw   目标航向角（度）
 * @note   只在直线赛道或遥控模式下有意义。
 *         MPU6500 无磁力计，Yaw 会缓慢漂移，
 *         因此这个功能适合"几秒内保持直线"，不适合长时间绝对定向。
 */
void Control_SetYawHold(uint8_t enable, float target_yaw);

/** 以当前航向作为锁定基准 */
void Control_CaptureYaw(void);

#ifdef __cplusplus
}
#endif

#endif /* __CONTROL_H__ */
