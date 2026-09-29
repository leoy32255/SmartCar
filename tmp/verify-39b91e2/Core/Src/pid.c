/**
  ******************************************************************************
  * @file    pid.c
  * @brief   通用 PID 实现（整数运算）
  *
  *          系数约定：kp/ki/kd 都以 ×100 的整数传入。
  *          例如 kp = 30 表示实际增益 0.30。
  *          这样既能用整数运算，又保留了两位小数的调节分辨率。
  ******************************************************************************
  */

#include "pid.h"

#define PID_SCALE       100     /* 系数放大倍数 */

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v > hi) return hi;
    if (v < lo) return lo;
    return v;
}

/* ==========================================================================
 * 位置式
 * ========================================================================== */

void Pid_Init(Pid_t *pid, int32_t kp, int32_t ki, int32_t kd,
              int32_t out_max, int32_t integral_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_max = out_max;
    pid->integral_max = integral_max;
    pid->integral = 0;
    pid->last_error = 0;
    pid->last_measure = 0;
    pid->deadband = 0;
    pid->integral_sep_th = 0;       /* 0 = 不做积分分离 */
    pid->use_measure_deriv = 0;
    pid->first_run = 1;
}

void Pid_Reset(Pid_t *pid)
{
    pid->integral = 0;
    pid->last_error = 0;
    pid->last_measure = 0;
    pid->first_run = 1;
}

void Pid_SetTunings(Pid_t *pid, int32_t kp, int32_t ki, int32_t kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void Pid_SetLimits(Pid_t *pid, int32_t deadband, int32_t integral_sep_th)
{
    pid->deadband = deadband;
    pid->integral_sep_th = integral_sep_th;
}

int32_t Pid_Calc(Pid_t *pid, int32_t target, int32_t measure)
{
    int32_t error = target - measure;
    int32_t p_term, i_term, d_term;

    /* ---- 死区：小误差不动作，避免执行器在目标附近来回抖 ---- */
    if (pid->deadband > 0 && error > -pid->deadband && error < pid->deadband) {
        error = 0;
    }

    /* ---- 比例项 ---- */
    p_term = pid->kp * error;

    /* ---- 积分项（带分离 + 限幅，防止饱和） ---- */
    if (pid->integral_sep_th > 0) {
        /* 误差过大时不积分：远离目标时积分会迅速堆满，
         * 等接近目标后需要很久才能"卸掉"，表现为大幅超调和回摆。 */
        if (error > -pid->integral_sep_th && error < pid->integral_sep_th) {
            pid->integral += error;
        }
    } else {
        pid->integral += error;
    }

    pid->integral = clamp_i32(pid->integral, -pid->integral_max, pid->integral_max);
    i_term = pid->ki * pid->integral;

    /* ---- 微分项 ---- */
    if (pid->first_run) {
        d_term = 0;
        pid->first_run = 0;
    } else if (pid->use_measure_deriv) {
        /* 微分先行：对测量值微分而不是误差，
         * 这样设定值突然跳变时不会产生"微分冲击"（derivative kick）。 */
        d_term = 0 - (pid->kd * (measure - pid->last_measure));
    } else {
        d_term = pid->kd * (error - pid->last_error);
    }

    pid->last_error = error;
    pid->last_measure = measure;

    /* 系数是 ×100 的，所以结果要除掉 */
    return clamp_i32((p_term + i_term + d_term) / PID_SCALE,
                     -pid->out_max, pid->out_max);
}

/* ==========================================================================
 * 增量式
 * ========================================================================== */

void PidInc_Init(PidInc_t *pid, int32_t kp, int32_t ki, int32_t kd, int32_t out_max)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_max = out_max;
    pid->out = 0;
    pid->last_error = 0;
    pid->prev_error = 0;
    pid->first_run = 1;
}

void PidInc_Reset(PidInc_t *pid)
{
    pid->out = 0;
    pid->last_error = 0;
    pid->prev_error = 0;
    pid->first_run = 1;
}

void PidInc_SetTunings(PidInc_t *pid, int32_t kp, int32_t ki, int32_t kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
}

void PidInc_SetOutput(PidInc_t *pid, int32_t out)
{
    pid->out = clamp_i32(out, -pid->out_max, pid->out_max);
}

int32_t PidInc_Calc(PidInc_t *pid, int32_t target, int32_t measure)
{
    int32_t error = target - measure;
    int32_t delta;
    int32_t new_out;

    if (pid->first_run) {
        /* 首次不让微分/积分项起作用，直接把当前输出作为起点，
         * 否则从 0 起步会产生一次很大的增量冲击。 */
        pid->first_run = 0;
        pid->last_error = error;
        pid->prev_error = error;
        return pid->out;
    }

    /* Δu = Kp·(e[k]-e[k-1]) + Ki·e[k] + Kd·(e[k]-2e[k-1]+e[k-2]) */
    delta = pid->kp * (error - pid->last_error)
          + pid->ki * error
          + pid->kd * ((error - pid->last_error) - (pid->last_error - pid->prev_error));

    delta /= PID_SCALE;

    pid->prev_error = pid->last_error;
    pid->last_error = error;

    new_out = pid->out + delta;

    /* 先限幅输出，再把限幅后的值写回累加器。
     * 这样在持续限幅期间累加器不会继续涨，退出限幅时不会爆冲。
     * 注意不要在这里把 delta 清零，否则会丢失"恢复方向"的信息。 */
    if (new_out > pid->out_max) {
        new_out = pid->out_max;
    } else if (new_out < -pid->out_max) {
        new_out = -pid->out_max;
    }

    pid->out = new_out;
    return pid->out;
}
