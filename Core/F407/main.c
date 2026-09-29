#include "bsp_config.h"

/* ST Reset_Handler initializes data/bss and calls newlib's constructor runner.
 * No generic CRT startup (and no semihosting/syscall runtime) is needed. */
void _init(void) {}

int main(void)
{
    if (HAL_Init() != HAL_OK) Error_Handler();
    Bsp_Init();
    /* Bring-up only: no legacy App_Init, autostart, or motion command path. */
    for (;;) __WFI();
}

void Error_Handler(void)
{
    __disable_irq();
    /* Also safe if invoked before the normal GPIO/timer initialization. */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    HAL_GPIO_WritePin(BSP_MOTOR_PORT, BSP_MOTOR_DIRECTIONS, GPIO_PIN_SET);
    GPIO_InitTypeDef pins = {0};
    pins.Pin = BSP_MOTOR_DIRECTIONS;
    pins.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(BSP_MOTOR_PORT, &pins);
    HAL_GPIO_WritePin(BSP_MOTOR_PORT, BSP_MOTOR_PWM_PINS, GPIO_PIN_RESET);
    pins.Pin = BSP_MOTOR_PWM_PINS;
    HAL_GPIO_Init(BSP_MOTOR_PORT, &pins);
    for (;;) __NOP();
}
