#include "bsp_config.h"
#include "motor.h"

static uint8_t enabled;
static int16_t applied_left, applied_right;

static int16_t limit(int16_t value)
{
    if (value > BSP_MOTOR_MAX_PERMILLE) return BSP_MOTOR_MAX_PERMILLE;
    if (value < -BSP_MOTOR_MAX_PERMILLE) return -BSP_MOTOR_MAX_PERMILLE;
    if (value < BSP_MOTOR_MIN_PERMILLE && value > -BSP_MOTOR_MIN_PERMILLE) return 0;
    return value;
}

static int direction(int16_t value) { return (value > 0) - (value < 0); }

static void set_direction(int16_t value, uint16_t in1, uint16_t in2)
{
    /* Both pins were first set to 11 by Bsp_MotorStop. Never traverse 00. */
    if (value) HAL_GPIO_WritePin(BSP_MOTOR_PORT, value > 0 ? in2 : in1, GPIO_PIN_RESET);
}

static uint32_t compare(int16_t value)
{
    uint32_t magnitude = (uint32_t)(value < 0 ? -(int32_t)value : value);
    return magnitude * (BSP_PWM_ARR + 1U) / 1000U;
}

void Motor_Stop(void)
{
    enabled = 0;
    Bsp_MotorStop();
    applied_left = applied_right = 0;
}

void Motor_Init(void) { Motor_Stop(); }

void Motor_Enable(uint8_t enable)
{
    if (!enable) Motor_Stop();
    else enabled = 1;
}

void Motor_SetPWM(int16_t left, int16_t right)
{
    if (!enabled) {
        Motor_Stop();
        return;
    }
    left = limit(left);
    right = limit(right);
    if (direction(left) != direction(applied_left) ||
        direction(right) != direction(applied_right)) {
        /* Blank both bridges and flush old preloads before changing polarity.
         * New duty starts at the next 2 kHz wrap; stable-direction updates
         * retain the timer phase and use the normal OC preload mechanism. */
        Bsp_MotorStop();
        set_direction(BSP_MOTOR_LEFT_INVERT ? -left : left,
                      BSP_MOTOR_LEFT_IN1, BSP_MOTOR_LEFT_IN2);
        set_direction(BSP_MOTOR_RIGHT_INVERT ? -right : right,
                      BSP_MOTOR_RIGHT_IN1, BSP_MOTOR_RIGHT_IN2);
    }
    BSP_MOTOR_PWM_TIMER->CCR1 = compare(left);
    BSP_MOTOR_PWM_TIMER->CCR2 = compare(right);
    applied_left = left;
    applied_right = right;
}

void Motor_GetPWM(int16_t *left, int16_t *right)
{
    if (left) *left = applied_left;
    if (right) *right = applied_right;
}
