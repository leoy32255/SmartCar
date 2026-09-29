/**
  ******************************************************************************
  * @file    track.h
  * @brief   5 路 TCRT5000 数字循迹传感器接口
  *
  *          模块输出真值（按实测采购的 TCRT5000 模块手册）：
  *            压到黑线        -> 低电平
  *            压在白色地面    -> 高电平
  *            超出探测距离    -> 低电平  ← 注意！和"黑线"同电平
  *
  *          因此"全部低"是歧义的：可能是压到大片黑色，也可能是车被抬起/
  *          冲出赛道边缘。本驱动把这两种情况区分开处理（见 Track_Status_t）。
  *
  *          偏差计算方法：加权重心法（Center of Gravity）
  *            偏差 = Σ(权重_i × 第 i 路有效) / Σ(第 i 路有效)
  *          权重取 -200/-100/0/+100/+200，即 1/100 的传感器间距。
  *          左负右正 —— 负值表示线在车左侧，需要向左修正。
  ******************************************************************************
  */

#ifndef __TRACK_H__
#define __TRACK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 循迹状态 */
typedef enum {
    TRACK_STATUS_OK       = 0,  /* 正常检测到黑线（1~4 路有效） */
    TRACK_STATUS_LOST     = 1,  /* 全部读到白色：线丢了，跑到白地上了 */
    TRACK_STATUS_ALL      = 2,  /* 全部读到"黑"：压到大片黑区，或被抬起/出界 */
    TRACK_STATUS_SUSPECT  = 3   /* 上面两种异常已持续过久，建议停车 */
} Track_Status_t;

/**
 * @brief  循迹模块初始化（引脚已在 Bsp_Init 配好）
 */
void Track_Init(void);

/**
 * @brief  采样一次 5 路电平并更新偏差
 * @note   每个控制周期调用一次。内部做 3 取样 2 表决的中值滤波，
 *         所以必须按周期连续调用，不能只在需要时才调。
 */
void Track_ReadSensors(void);

/**
 * @brief  取当前横向偏差
 * @retval -200 ~ +200（1/100 传感器间距），负 = 线偏左，正 = 线偏右
 *         丢线时返回最后一次有效偏差（便于按原方向找回）
 */
int16_t Track_GetDeviation(void);

/** 原始有效位图：bit0 = OUT1(最左) ... bit4 = OUT5(最右)，1 = 压到黑线 */
uint8_t Track_GetMask(void);

/** 压到黑线的路数 (0~5) */
uint8_t Track_GetActiveCount(void);

/** 当前循迹状态 */
Track_Status_t Track_GetStatus(void);

/**
 * @brief  复位丢线计数与历史值（重新出发时调用）
 */
void Track_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __TRACK_H__ */
