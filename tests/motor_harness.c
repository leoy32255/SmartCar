#include "host_assert.h"
#include <stdio.h>
#include "bsp_config.h"
#include "motor.h"

static void expect_stopped(void)
{
    int16_t left = -1, right = -1;
    Motor_GetPWM(&left, &right);
    assert(left == 0 && right == 0);
    assert(TIM1->CCR1 == 0 && TIM1->CCR2 == 0);
    assert((GPIOE->ODR & 0x3500U) == 0x3500U);
}

static void expect_output(int16_t left, int16_t right, uint32_t directions,
                          uint32_t left_ccr, uint32_t right_ccr)
{
    int16_t actual_left, actual_right;
    Motor_GetPWM(&actual_left, &actual_right);
    assert(actual_left == left && actual_right == right);
    /* The two correction switches swap the physical pins, not telemetry signs. */
    if (BSP_MOTOR_LEFT_INVERT && left) directions ^= 0x0500U;
    if (BSP_MOTOR_RIGHT_INVERT && right) directions ^= 0x3000U;
    assert((GPIOE->ODR & 0x3500U) == directions);
    assert(TIM1->CCR1 == left_ccr && TIM1->CCR2 == right_ccr);
}

void test_motor(void)
{
    TIM3->CNT = 12345;
    TIM4->CNT = 54321;
    /* Startup ignores a command until explicit enable, including before Init. */
    Motor_SetPWM(400, -500);
    expect_stopped();
    Motor_Init();
    Motor_SetPWM(400, -500);
    expect_stopped();
    Motor_Enable(1);
    expect_stopped();
    Motor_SetPWM(50, -950);
    expect_output(50, -950, 0x2400U, 50, 950);
    Motor_SetPWM(32767, -32768);
    expect_output(950, -950, 0x2400U, 950, 950);
    Motor_SetPWM(951, -951);
    expect_output(950, -950, 0x2400U, 950, 950);
    Motor_SetPWM(-50, 950);
    expect_output(-50, 950, 0x1100U, 50, 950);
    Motor_SetPWM(-501, 249);
    expect_output(-501, 249, 0x1100U, 501, 249);
    Motor_SetPWM(49, -49);
    expect_stopped();
    Motor_SetPWM(-1, 1);
    expect_stopped();
    Motor_SetPWM(0, 700);
    expect_output(0, 700, 0x1500U, 0, 700);
    Motor_SetPWM(-800, 0);
    expect_output(-800, 0, 0x3100U, 800, 0);
    Motor_SetPWM(0, 0);
    expect_stopped();
    Motor_SetPWM(500, -600);
    TIM1->EGR = 0; /* Forget the preceding update event. */
    Motor_SetPWM(-500, 600);
    assert(TIM1->EGR == TIM_EGR_UG); /* Discard old shadow duty before new direction. */
    expect_output(-500, 600, 0x1100U, 500, 600);
    TIM1->EGR = 0;
    Motor_SetPWM(-300, 400);
    assert(TIM1->EGR == 0); /* Same-direction updates must not reset PWM phase. */
    expect_output(-300, 400, 0x1100U, 300, 400);
    Motor_Enable(1); /* Repeated enable leaves current drive unchanged. */
    expect_output(-300, 400, 0x1100U, 300, 400);
    Motor_Enable(0);
    expect_stopped();
    Motor_SetPWM(900, 900);
    expect_stopped();
    Motor_Enable(1);
    expect_stopped();
    Motor_SetPWM(100, 200);
    Motor_Stop();
    expect_stopped();
    Motor_SetPWM(900, 900);
    expect_stopped();
    Motor_Enable(1);
    expect_stopped();
    Motor_SetPWM(100, 200);
    Motor_Init();
    Motor_SetPWM(100, 200);
    expect_stopped();
    Motor_GetPWM(NULL, NULL);
    assert(TIM1->PSC == 83 && TIM1->ARR == 999);
    assert(TIM3->CNT == 12345 && TIM4->CNT == 54321);
    assert(GPIOE->ODR & GPIO_PIN_7); /* MCP23S17 stays deselected. */
    assert(GPIOB->ODR & GPIO_PIN_12); /* IMU stays deselected. */
    puts("Motor direction and duty-boundary checks passed");
}
