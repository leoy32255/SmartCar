/**
  ******************************************************************************
  * @file    control.c
  * @brief   串级 PID 运动控制实现
  ******************************************************************************
  */

#include "control.h"
#include "pid.h"
#include "track.h"
#include "encoder.h"
#include "motor.h"
#include "filter.h"

/* ---- 可在线调整的参数 ---- */
static int16_t s_base_speed  = CFG_BASE_SPEED_DEFAULT;
static int16_t s_track_kp    = CFG_TRACK_KP_DEFAULT;
static int16_t s_track_kd    = CFG_TRACK_KD_DEFAULT;
static int16_t s_speed_kp    = CFG_SPEED_KP_DEFAULT;
static int16_t s_speed_ki    = CFG_SPEED_KI_DEFAULT;
static int16_t s_speed_kd    = CFG_SPEED_KD_DEFAULT;

/* ---- PID 实例 ---- */
static Pid_t    s_track_pid;        /* 外环：循迹 PD */
static PidInc_t s_speed_pid_l;      /* 内环：左轮速度 */
static PidInc_t s_speed_pid_r;      /* 内环：右轮速度 */

/* ---- 本轮中间量 ---- */
static int16_t s_turn         = 0;
static int16_t s_target_l     = 0;
static int16_t s_target_r     = 0;

/* ---- 航向锁定 ---- */
static uint8_t s_yaw_hold_en  = 0;
static float   s_yaw_target   = 0.0f;
static int32_t s_yaw_turn     = 0;      /* Yaw 环输出的差速修正量 */

/* ---- 直控模式（遥控用）：跳过循迹外环 ---- */
static uint8_t s_direct_mode  = 0;
static int16_t s_direct_l     = 0;
static int16_t s_direct_r     = 0;

/** 差速量上限：限制最大转向力度，同时允许原地转向
 *  （turn 可以超过 base_speed，此时一侧轮反转） */
#define CONTROL_TURN_LIMIT      (MOTOR_PWM_MAX / 2)
#define CONTROL_TARGET_LIMIT    200     /* 目标速度上限 counts/窗口，防止调到飞车 */

/* Yaw 环：比例增益 ×100，输出单位为 counts/窗口 */
#define CONTROL_YAW_KP          12
/** Yaw 环最大差速贡献，避免航向修正把循迹修正完全压掉 */
#define CONTROL_YAW_TURN_LIMIT  60

/* ========================================================================== */

void Control_Init(void)
{
    /* 外环：PD。纯 P 会在过弯时"提前量"不足，D 项提供阻尼。
     * ki = 0 —— 循迹误差不需要积分，理由见 control.h 顶部说明。 */
    Pid_Init(&s_track_pid, s_track_kp, 0, s_track_kd,
             CONTROL_TURN_LIMIT, 0);
    s_track_pid.use_measure_deriv = 1;      /* 对偏差微分，抑制设定值跳变冲击 */
    s_track_pid.deadband = 0;               /* 循迹死区不设，否则中线附近不修 */

    /* 内环：增量式，输出直接是 PWM (0~1000) */
    PidInc_Init(&s_speed_pid_l, s_speed_kp, s_speed_ki, s_speed_kd, MOTOR_PWM_MAX);
    PidInc_Init(&s_speed_pid_r, s_speed_kp, s_speed_ki, s_speed_kd, MOTOR_PWM_MAX);

    s_turn = 0;
    s_target_l = s_target_r = 0;
    s_yaw_hold_en = 0;
    s_yaw_target = 0.0f;
    s_yaw_turn = 0;
    s_direct_mode = 0;
    s_direct_l = s_direct_r = 0;
}

void Control_Update(void)
{
    int16_t dev;
    int16_t speed_l;
    int16_t speed_r;
    int32_t turn;
    int32_t pwm_l;
    int32_t pwm_r;

    if (s_direct_mode) {
        /* ---- 直控模式（遥控）：外环完全不参与，直接给定两轮目标速度 ----
         * 内环速度 PID 仍然在跑，所以给定速度和实际速度之间依然闭环。 */
        s_turn     = 0;
        s_yaw_turn = 0;
        s_target_l = s_direct_l;
        s_target_r = s_direct_r;

    } else {
        /* ---- 1. 外环：循迹偏差 -> 差速量 ----
         * 语义：target = 0，measure = 偏差。
         *   偏差 > 0（线偏右）  -> error < 0 -> turn < 0
         *   配合下面 left = base - turn / right = base + turn，
         *   turn < 0 时左轮加速、右轮减速，车向右转，修正正确。
         * 偏差为负则反之。若整车循迹方向反了，改 track.h 里的 TRACK_DIR_SIGN 即可。 */
        dev = Track_GetDeviation();
        turn = Pid_Calc(&s_track_pid, 0, dev);
        s_turn = (int16_t)turn;

        /* ---- 2. 航向锁定叠加（可选） ---- */
        s_yaw_turn = 0;
        if (s_yaw_hold_en) {
            float yaw_err = s_yaw_target - Filter_GetYaw();

            /* 角度回绕：走最短路径。例如目标 -170°、当前 +170°，
             * 实际只差 20°，若不处理会算出 -340°，车会朝反方向猛转。 */
            while (yaw_err >  180.0f) yaw_err -= 360.0f;
            while (yaw_err <= -180.0f) yaw_err += 360.0f;

            s_yaw_turn = (int32_t)(yaw_err * CONTROL_YAW_KP) / 100;

            if (s_yaw_turn >  CONTROL_YAW_TURN_LIMIT) s_yaw_turn =  CONTROL_YAW_TURN_LIMIT;
            if (s_yaw_turn < -CONTROL_YAW_TURN_LIMIT) s_yaw_turn = -CONTROL_YAW_TURN_LIMIT;
        }

        /* ---- 3. 合成两轮目标速度 ---- */
        {
            int32_t base = s_base_speed;
            int32_t t    = (int32_t)s_turn + s_yaw_turn;

            s_target_l = (int16_t)(base - t);
            s_target_r = (int16_t)(base + t);
        }
    }

    /* 目标限幅 */
    if (s_target_l >  CONTROL_TARGET_LIMIT) s_target_l =  CONTROL_TARGET_LIMIT;
    if (s_target_l < -CONTROL_TARGET_LIMIT) s_target_l = -CONTROL_TARGET_LIMIT;
    if (s_target_r >  CONTROL_TARGET_LIMIT) s_target_r =  CONTROL_TARGET_LIMIT;
    if (s_target_r < -CONTROL_TARGET_LIMIT) s_target_r = -CONTROL_TARGET_LIMIT;

    /* ---- 4. 内环：实际转速 -> PWM ---- */
    speed_l = Encoder_GetLeftSpeed();
    speed_r = Encoder_GetRightSpeed();

    pwm_l = PidInc_Calc(&s_speed_pid_l, s_target_l, speed_l);
    pwm_r = PidInc_Calc(&s_speed_pid_r, s_target_r, speed_r);

    /* ---- 5. 输出 ----
     * 左轮 PWM <- 左轮速度环，右轮 PWM <- 右轮速度环，直接对应，不做交换。
     * 若实测整车前进变后退，说明电机线序接了反，
     * 改 motor.h 里的 MOTOR_LEFT_INVERT / MOTOR_RIGHT_INVERT，不要在这里交换。 */
    Motor_SetPWM((int16_t)pwm_l, (int16_t)pwm_r);
}

void Control_Stop(void)
{
    Motor_Coast();

    Pid_Reset(&s_track_pid);
    PidInc_Reset(&s_speed_pid_l);
    PidInc_Reset(&s_speed_pid_r);

    s_turn = 0;
    s_target_l = s_target_r = 0;
    s_yaw_turn = 0;
    s_direct_l = s_direct_r = 0;
}

void Control_Brake(void)
{
    Motor_Brake();

    Pid_Reset(&s_track_pid);
    PidInc_Reset(&s_speed_pid_l);
    PidInc_Reset(&s_speed_pid_r);

    s_turn = 0;
    s_target_l = s_target_r = 0;
    s_yaw_turn = 0;
}

/* ==========================================================================
 * 直控模式（遥控）
 * ========================================================================== */

void Control_SetDirectMode(uint8_t enable)
{
    if (enable && !s_direct_mode) {
        /* 从循迹切到直控：把外环复位，避免残留的 turn 值造成一次跳变 */
        Pid_Reset(&s_track_pid);
        s_turn = 0;
        s_yaw_turn = 0;
    }
    if (!enable && s_direct_mode) {
        /* 从直控切回循迹：速度环从当前输出平滑接手，不清零 */
        s_direct_l = s_direct_r = 0;
    }
    s_direct_mode = enable ? 1U : 0U;
}

void Control_SetDirectTarget(int16_t left, int16_t right)
{
    if (left >  CONTROL_TARGET_LIMIT) left =  CONTROL_TARGET_LIMIT;
    if (left < -CONTROL_TARGET_LIMIT) left = -CONTROL_TARGET_LIMIT;
    if (right >  CONTROL_TARGET_LIMIT) right =  CONTROL_TARGET_LIMIT;
    if (right < -CONTROL_TARGET_LIMIT) right = -CONTROL_TARGET_LIMIT;

    s_direct_l = left;
    s_direct_r = right;
}

/* ==========================================================================
 * 参数访问
 * ========================================================================== */

void Control_SetBaseSpeed(int16_t speed)
{
    if (speed < 0) speed = 0;
    if (speed > CONTROL_TARGET_LIMIT) speed = CONTROL_TARGET_LIMIT;
    s_base_speed = speed;
}

int16_t Control_GetBaseSpeed(void) { return s_base_speed; }

uint8_t Control_SetParam(Control_ParamId_t id, int16_t value)
{
    switch (id) {
    case PARAM_BASE_SPEED:
        Control_SetBaseSpeed(value);
        break;
    case PARAM_TRACK_KP:
        if (value < 0) return 1;
        s_track_kp = value;
        Pid_SetTunings(&s_track_pid, s_track_kp, 0, s_track_kd);
        break;
    case PARAM_TRACK_KD:
        if (value < 0) return 1;
        s_track_kd = value;
        Pid_SetTunings(&s_track_pid, s_track_kp, 0, s_track_kd);
        break;
    case PARAM_SPEED_KP:
        if (value < 0) return 1;
        s_speed_kp = value;
        PidInc_SetTunings(&s_speed_pid_l, s_speed_kp, s_speed_ki, s_speed_kd);
        PidInc_SetTunings(&s_speed_pid_r, s_speed_kp, s_speed_ki, s_speed_kd);
        break;
    case PARAM_SPEED_KI:
        if (value < 0) return 1;
        s_speed_ki = value;
        PidInc_SetTunings(&s_speed_pid_l, s_speed_kp, s_speed_ki, s_speed_kd);
        PidInc_SetTunings(&s_speed_pid_r, s_speed_kp, s_speed_ki, s_speed_kd);
        break;
    case PARAM_SPEED_KD:
        if (value < 0) return 1;
        s_speed_kd = value;
        PidInc_SetTunings(&s_speed_pid_l, s_speed_kp, s_speed_ki, s_speed_kd);
        PidInc_SetTunings(&s_speed_pid_r, s_speed_kp, s_speed_ki, s_speed_kd);
        break;
    default:
        return 1;
    }
    return 0;
}

int16_t Control_GetParam(Control_ParamId_t id)
{
    switch (id) {
    case PARAM_BASE_SPEED:  return s_base_speed;
    case PARAM_TRACK_KP:    return s_track_kp;
    case PARAM_TRACK_KD:    return s_track_kd;
    case PARAM_SPEED_KP:    return s_speed_kp;
    case PARAM_SPEED_KI:    return s_speed_ki;
    case PARAM_SPEED_KD:    return s_speed_kd;
    default:                return 0;
    }
}

void Control_GetTargetSpeed(int16_t *left, int16_t *right)
{
    if (left)  *left  = s_target_l;
    if (right) *right = s_target_r;
}

void Control_GetActualSpeed(int16_t *left, int16_t *right)
{
    if (left)  *left  = Encoder_GetLeftSpeed();
    if (right) *right = Encoder_GetRightSpeed();
}

int16_t Control_GetTurn(void) { return s_turn; }

/* ==========================================================================
 * 航向锁定
 * ========================================================================== */

void Control_SetYawHold(uint8_t enable, float target_yaw)
{
    s_yaw_hold_en = enable ? 1U : 0U;

    if (enable) {
        /* 把目标角规范化到 (-180, 180] */
        while (target_yaw >  180.0f) target_yaw -= 360.0f;
        while (target_yaw <= -180.0f) target_yaw += 360.0f;
        s_yaw_target = target_yaw;
    } else {
        s_yaw_turn = 0;
    }
}

void Control_CaptureYaw(void)
{
    s_yaw_target = Filter_GetYaw();
}
