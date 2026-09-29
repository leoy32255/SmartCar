/**
  ******************************************************************************
  * @file    encoder.c
  * @brief   MG520 正交编码器测速实现
  *
  *          两个关键实现点：
  *          1. TIM2/TIM3 是 16 位计数器，会自然回绕（32767 -> -32768）。
  *             直接相减在回绕点会得到 ±65535 的野值，必须做回绕修正。
  *          2. 速度不能只看单个控制周期的计数（量化噪声大），
  *             所以累加 SPEED_WINDOW_TICKS 个周期再发布一次速度值。
  ******************************************************************************
  */

#include "encoder.h"

/* 上一周期的原始计数值 */
static uint16_t s_last_cnt_l = 0;
static uint16_t s_last_cnt_r = 0;

/* 本控制周期的增量（已修正符号） */
static int16_t  s_delta_l = 0;
static int16_t  s_delta_r = 0;

/* 速度窗口累加器 */
static int16_t  s_win_acc_l = 0;
static int16_t  s_win_acc_r = 0;
static uint8_t  s_win_cnt   = 0;

/* 已发布的速度值（counts / 窗口） */
static int16_t  s_speed_l = 0;
static int16_t  s_speed_r = 0;

/* 累计里程 */
static int32_t  s_total_l = 0;
static int32_t  s_total_r = 0;

/* ==========================================================================
 * 16 位回绕修正：把 uint16 差值解释成有符号 16 位增量
 *   例：last=65530, now=5  ->  diff = 5 - 65530 = -65525
 *       转成 int16_t 后 = 11，即正向走了 11 个计数（正确）
 * ========================================================================== */
static int16_t wrap_delta(uint16_t now, uint16_t last)
{
    return (int16_t)(now - last);
}

/* ========================================================================== */

void Encoder_Init(void)
{
    __HAL_TIM_SET_COUNTER(ENCODER_HTIM_LEFT,  0);
    __HAL_TIM_SET_COUNTER(ENCODER_HTIM_RIGHT, 0);
    Encoder_Reset();

    s_last_cnt_l = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_HTIM_LEFT);
    s_last_cnt_r = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_HTIM_RIGHT);
}

void Encoder_Update(void)
{
    uint16_t now_l = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_HTIM_LEFT);
    uint16_t now_r = (uint16_t)__HAL_TIM_GET_COUNTER(ENCODER_HTIM_RIGHT);

    int16_t d_l = wrap_delta(now_l, s_last_cnt_l);
    int16_t d_r = wrap_delta(now_r, s_last_cnt_r);

    s_last_cnt_l = now_l;
    s_last_cnt_r = now_r;

#if ENCODER_LEFT_INVERT
    d_l = (int16_t)(-d_l);
#endif
#if ENCODER_RIGHT_INVERT
    d_r = (int16_t)(-d_r);
#endif

    s_delta_l = d_l;
    s_delta_r = d_r;

    s_total_l += d_l;
    s_total_r += d_r;

    /* 累加到速度窗口，满窗口后发布并重新开始累加 */
    s_win_acc_l += d_l;
    s_win_acc_r += d_r;
    s_win_cnt++;

    if (s_win_cnt >= SPEED_WINDOW_TICKS) {
        s_speed_l = s_win_acc_l;
        s_speed_r = s_win_acc_r;
        s_win_acc_l = 0;
        s_win_acc_r = 0;
        s_win_cnt   = 0;
    }
}

int16_t Encoder_GetLeftDelta(void)  { return s_delta_l; }
int16_t Encoder_GetRightDelta(void) { return s_delta_r; }

int16_t Encoder_GetLeftSpeed(void)  { return s_speed_l; }
int16_t Encoder_GetRightSpeed(void) { return s_speed_r; }

float Encoder_GetLeftRPM(void)
{
    return (float)s_speed_l * ENCODER_COUNTS_TO_RPM;
}

float Encoder_GetRightRPM(void)
{
    return (float)s_speed_r * ENCODER_COUNTS_TO_RPM;
}

int32_t Encoder_GetLeftTotal(void)  { return s_total_l; }
int32_t Encoder_GetRightTotal(void) { return s_total_r; }

void Encoder_Reset(void)
{
    s_delta_l = s_delta_r = 0;
    s_win_acc_l = s_win_acc_r = 0;
    s_win_cnt = 0;
    s_speed_l = s_speed_r = 0;
    s_total_l = s_total_r = 0;
}
