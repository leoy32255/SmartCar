/**
  ******************************************************************************
  * @file    encoder.h
  * @brief   MG520 正交编码器测速接口（TIM2 左轮 / TIM3 右轮，4 倍频）
  *
  *          分辨率说明：
  *            输出轴一圈 = 11 线 × 30 减速比 × 4 倍频 = 1320 counts
  *            速度窗口 = SPEED_WINDOW_MS (默认 10ms)
  *            额定 180rpm 时窗口内约 40 个计数，
  *            量化误差 ±1 count ≈ ±2.5%，速度环可以接受。
  *
  *          速度单位统一为 "counts / 速度窗口"，这是给 PID 用的最自然单位，
  *          不引入浮点误差累积；需要显示时才用 Encoder_GetSpeedRPM() 换算。
  ******************************************************************************
  */

#ifndef __ENCODER_H__
#define __ENCODER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 编码器方向修正：1 = 该轮计数符号取反 */
#define ENCODER_LEFT_INVERT     0
#define ENCODER_RIGHT_INVERT    0

/**
 * @brief  编码器模块初始化（定时器已在 Bsp_Init 启动，这里清零累计量）
 */
void Encoder_Init(void);

/**
 * @brief  每个控制周期调用一次：读取本次计数增量
 * @note   由 App 的 5ms 节拍调用，不要放在中断里重复调用。
 */
void Encoder_Update(void);

/* ---- 原始增量：本控制周期内新增的计数 -------------------------------- */
int16_t Encoder_GetLeftDelta(void);
int16_t Encoder_GetRightDelta(void);

/* ---- 速度：counts / 速度窗口，供速度环 PID 使用 ---------------------- */
int16_t Encoder_GetLeftSpeed(void);
int16_t Encoder_GetRightSpeed(void);

/** 换算成输出轴转速 (rpm)，带符号 */
float Encoder_GetLeftRPM(void);
float Encoder_GetRightRPM(void);

/** 累计里程（counts，int32 不回绕），可用于里程计回传 */
int32_t Encoder_GetLeftTotal(void);
int32_t Encoder_GetRightTotal(void);

/** 清空速度窗口与累计量（切换模式/重新出发时调用） */
void Encoder_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H__ */
