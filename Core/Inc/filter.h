/**
  ******************************************************************************
  * @file    filter.h
  * @brief   姿态解算：互补滤波（加速度计 + 陀螺仪融合）
  *
  *          为什么用互补滤波而不是卡尔曼：
  *            - 加速度计低频准、高频噪声大；陀螺仪高频准、低频积分漂移；
  *              两者频段互补，一阶互补滤波就能得到很好的结果；
  *            - 运算量只有几十个浮点操作，5ms 周期下 72MHz 的 M3 毫无压力，
  *              而卡尔曼要做矩阵运算，对 20KB RAM 的 F103C8T6 不划算。
  *
  *          重要限制：MPU6500 没有磁力计，Yaw 没有绝对参考，
  *          只能靠陀螺 Z 轴积分，长时间必定漂移（约 1~5°/分钟，取决于零偏校准）。
  *          所以 Yaw 只适合做"短时航向锁定"（几秒~几十秒），
  *          不能当作绝对方向使用。需要绝对航向必须换带磁力计的 ICM-20948 / MPU9250。
  ******************************************************************************
  */

#ifndef __FILTER_H__
#define __FILTER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/**
 * @brief  互补滤波系数 alpha
 *         角度 = alpha × (上一角度 + 陀螺积分) + (1-alpha) × 加计观测角
 *         alpha 越接近 1 越信陀螺（响应快但会漂），越接近 0 越信加计（稳但迟钝）。
 *         0.98 对应约 0.5s 的时间常数，是 5ms 周期下的常用值。
 */
#define FILTER_ALPHA_DEFAULT    0.98f

/**
 * @brief  滤波模块初始化（复位角度与时间基准）
 */
void Filter_Init(void);

/**
 * @brief  用最新一帧 IMU 数据更新姿态角
 * @note   在控制周期内、Imu_ReadData() 之后调用一次。
 *         内部用固定 dt（CTRL_PERIOD_MS），不依赖 DWT 计时，行为可复现。
 */
void Filter_Update(void);

/** 姿态角，单位度。Yaw 范围 (-180, 180]，Roll/Pitch 范围约 (-90, 90) */
float Filter_GetYaw(void);
float Filter_GetPitch(void);
float Filter_GetRoll(void);

/** 航向角积分累计值（度，不回绕），用于里程/转向累计统计 */
float Filter_GetYawTotal(void);

/** 清零 Yaw 与累计值（例如把当前朝向设为 0° 基准） */
void Filter_ResetYaw(void);

/** 设置互补滤波系数（0.90 ~ 0.995） */
void Filter_SetAlpha(float alpha);

#ifdef __cplusplus
}
#endif

#endif /* __FILTER_H__ */
