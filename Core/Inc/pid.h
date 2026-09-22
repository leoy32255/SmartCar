/**
  ******************************************************************************
  * @file    pid.h
  * @brief   通用 PID 控制器（位置式 + 增量式）
  *
  *          两种形式各自的适用场景：
  *            位置式：输出直接是控制量。本项目用于循迹外环（PD），
  *                    因为要的是"偏差 -> 差速量"的静态映射，不需要累积。
  *            增量式：输出是上一次输出的增量。本项目用于速度内环，
  *                    因为增量式天然带积分（近似），且换向/限幅时不会
  *                    出现位置式的积分饱和爆冲，对电机控制更安全。
  *
  *          全部用整数运算，避免浮点在 5ms 中断/节拍里的开销与不可复现性。
  ******************************************************************************
  */

#ifndef __PID_H__
#define __PID_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==========================================================================
 * 位置式 PID
 * ========================================================================== */
typedef struct {
    int32_t kp;             /* 比例系数（放大 100 倍传入更细腻，比如 30 = 0.30） */
    int32_t ki;
    int32_t kd;

    int32_t integral;       /* 积分累加器 */
    int32_t integral_max;   /* 积分限幅（抗饱和） */
    int32_t out_max;        /* 输出限幅 */
    int32_t last_error;     /* 上次误差（微分用） */
    int32_t last_measure;   /* 上次测量值（微分先行用） */

    int32_t deadband;       /* 误差死区：|error| <= deadband 时按 0 处理 */
    int32_t integral_sep_th;/* 积分分离阈值：|error| > 该值时不累积分 */

    uint8_t use_measure_deriv;  /* 1 = 微分先行（对测量值微分，抑制设定值跳变冲击） */
    uint8_t first_run;
} Pid_t;

/**
 * @brief  初始化位置式 PID
 * @param  pid             控制器实例
 * @param  kp,ki,kd        系数（×100 标度）
 * @param  out_max         输出限幅（绝对值）
 * @param  integral_max    积分限幅（绝对值）
 */
void Pid_Init(Pid_t *pid, int32_t kp, int32_t ki, int32_t kd,
              int32_t out_max, int32_t integral_max);

/**
 * @brief  位置式 PID 计算
 * @param  pid        控制器实例
 * @param  target     目标值
 * @param  measure    测量值
 * @retval 控制量，已限幅到 ±out_max
 */
int32_t Pid_Calc(Pid_t *pid, int32_t target, int32_t measure);

/** 复位控制器内部状态（不清系数） */
void Pid_Reset(Pid_t *pid);

/** 在线改系数 */
void Pid_SetTunings(Pid_t *pid, int32_t kp, int32_t ki, int32_t kd);

/** 设置误差死区与积分分离阈值 */
void Pid_SetLimits(Pid_t *pid, int32_t deadband, int32_t integral_sep_th);

/* ==========================================================================
 * 增量式 PID
 * ========================================================================== */
typedef struct {
    int32_t kp;             /* ×100 标度 */
    int32_t ki;
    int32_t kd;

    int32_t out;            /* 当前输出（需要保持，增量是在它基础上累加） */
    int32_t out_max;        /* 输出限幅 */

    int32_t last_error;     /* e[k-1] */
    int32_t prev_error;     /* e[k-2] */

    uint8_t first_run;
} PidInc_t;

/**
 * @brief  初始化增量式 PID
 */
void PidInc_Init(PidInc_t *pid, int32_t kp, int32_t ki, int32_t kd, int32_t out_max);

/**
 * @brief  增量式 PID 计算
 * @retval 本周期应该输出的控制量绝对值（内部已累加增量并限幅）
 * @note   增量公式：Δu = Kp(e[k]-e[k-1]) + Ki·e[k] + Kd(e[k]-2e[k-1]+e[k-2])
 *         限幅策略：先限幅增量输出，再反向修正内部累加值，
 *         这样在限幅期间不会积累出"虚假"的大输出（windup）。
 */
int32_t PidInc_Calc(PidInc_t *pid, int32_t target, int32_t measure);

/** 复位内部状态 */
void PidInc_Reset(PidInc_t *pid);

/** 在线改系数 */
void PidInc_SetTunings(PidInc_t *pid, int32_t kp, int32_t ki, int32_t kd);

/** 强制设定当前输出（比如切换到速度环时用当前实际 PWM 起步，避免跳变） */
void PidInc_SetOutput(PidInc_t *pid, int32_t out);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H__ */
