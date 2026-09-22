/**
  ******************************************************************************
  * @file    motor.c
  * @brief   TB6612FNG 双路电机驱动实现
  *
  *          TB6612 真值表（每路独立）：
  *            IN1  IN2   PWM    结果
  *             0    0     x     滑行（H 桥关断）
  *             1    0    PWM    正转
  *             0    1    PWM    反转
  *             1    1     x     刹车（两端短接，快速停止）
  *
  *          另外 STBY 必须为高，否则芯片整片关断，无论 IN/PWM 怎么给都不动。
  ******************************************************************************
  */

#include "motor.h"

/* 当前 PWM 设定值，供遥测查询 */
static int16_t s_pwm_left  = 0;
static int16_t s_pwm_right = 0;

/* ---- 内部：一组 H 桥的方向控制 ---------------------------------------- */
static void set_bridge_dir(GPIO_TypeDef *p1, uint16_t pin1,
                           GPIO_TypeDef *p2, uint16_t pin2,
                           int8_t dir)
{
    switch (dir) {
    case 1:     /* 正转 */
        HAL_GPIO_WritePin(p1, pin1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(p2, pin2, GPIO_PIN_RESET);
        break;
    case -1:    /* 反转 */
        HAL_GPIO_WritePin(p1, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(p2, pin2, GPIO_PIN_SET);
        break;
    case 2:     /* 刹车 */
        HAL_GPIO_WritePin(p1, pin1, GPIO_PIN_SET);
        HAL_GPIO_WritePin(p2, pin2, GPIO_PIN_SET);
        break;
    case 0:     /* 滑行 */
    default:
        HAL_GPIO_WritePin(p1, pin1, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(p2, pin2, GPIO_PIN_RESET);
        break;
    }
}

/* ---- 内部：限幅 -------------------------------------------------------- */
static int16_t clamp_pwm(int16_t v)
{
    if (v >  MOTOR_PWM_MAX) return  MOTOR_PWM_MAX;
    if (v < -MOTOR_PWM_MAX) return -MOTOR_PWM_MAX;
    return v;
}

/* ========================================================================== */

void Motor_Init(void)
{
    Motor_Standby(0);           /* 先关断，防止初始化过程中乱转 */
    Motor_SetDir(0, 0);         /* 两路都滑行 */
    s_pwm_left  = 0;
    s_pwm_right = 0;
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_LEFT,  0);
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_RIGHT, 0);
}

void Motor_Standby(uint8_t enable)
{
#if MOTOR_STBY_ACTIVE_HIGH
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

void Motor_SetDir(int8_t left_dir, int8_t right_dir)
{
#if MOTOR_LEFT_INVERT
    left_dir = -left_dir;
#endif
#if MOTOR_RIGHT_INVERT
    right_dir = -right_dir;
#endif
    set_bridge_dir(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN,
                   MOTOR_AIN2_PORT, MOTOR_AIN2_PIN, left_dir);
    set_bridge_dir(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN,
                   MOTOR_BIN2_PORT, MOTOR_BIN2_PIN, right_dir);
}

void Motor_SetPWM(int16_t left, int16_t right)
{
    left  = clamp_pwm(left);
    right = clamp_pwm(right);

    /* 先定方向，再给 PWM。反过来会在换向瞬间出现一次反向冲击。 */
    Motor_SetDir((left  > 0) ? 1 : ((left  < 0) ? -1 : 0),
                 (right > 0) ? 1 : ((right < 0) ? -1 : 0));

    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_LEFT,  (uint32_t)((left  < 0) ? -left  : left));
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_RIGHT, (uint32_t)((right < 0) ? -right : right));

    s_pwm_left  = left;
    s_pwm_right = right;
}

void Motor_Brake(void)
{
    Motor_SetDir(2, 2);
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_LEFT,  0);
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_RIGHT, 0);
    s_pwm_left  = 0;
    s_pwm_right = 0;
}

void Motor_Coast(void)
{
    Motor_SetDir(0, 0);
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_LEFT,  0);
    __HAL_TIM_SET_COMPARE(MOTOR_PWM_HTIM, MOTOR_PWM_CH_RIGHT, 0);
    s_pwm_left  = 0;
    s_pwm_right = 0;
}

void Motor_GetPWM(int16_t *left, int16_t *right)
{
    if (left)  *left  = s_pwm_left;
    if (right) *right = s_pwm_right;
}
