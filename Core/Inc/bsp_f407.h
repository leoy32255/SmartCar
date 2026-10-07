#ifndef BSP_F407_H
#define BSP_F407_H

#include "stm32f4xx_hal.h"
#include "app_config_f407.h"

/* XY-160D revision 3. PA0/PA1, USB, SWD and oscillator pins are reserved. */
#define BSP_SYSCLK_HZ          168000000U
#define BSP_APB1_HZ             42000000U
#define BSP_APB2_HZ             84000000U
#define BSP_PWM_TIMER_HZ       168000000U
#define BSP_TICK_TIMER_HZ       84000000U
#define BSP_PWM_PSC                    83U
#define BSP_PWM_ARR                   999U
#define BSP_TICK_PSC                 8399U
#define BSP_TICK_ARR                   49U
#define BSP_CTRL_PERIOD_MS              5U
#define BSP_UART_BAUD                9600U

#define BSP_MOTOR_PORT GPIOE
#define BSP_MOTOR_LEFT_IN1 GPIO_PIN_10
#define BSP_MOTOR_LEFT_IN2 GPIO_PIN_8
#define BSP_MOTOR_RIGHT_IN1 GPIO_PIN_12
#define BSP_MOTOR_RIGHT_IN2 GPIO_PIN_13
#define BSP_MOTOR_DIRECTIONS (BSP_MOTOR_LEFT_IN1 | BSP_MOTOR_LEFT_IN2 | \
                              BSP_MOTOR_RIGHT_IN1 | BSP_MOTOR_RIGHT_IN2)
#define BSP_MOTOR_PWM_PINS (GPIO_PIN_9 | GPIO_PIN_11)
#define BSP_MOTOR_PWM_TIMER TIM1
#define BSP_MOTOR_MIN_PERMILLE 50
#define BSP_MOTOR_MAX_PERMILLE 950
/* Software convention only; verify forward wheel direction on the bench. */
#ifndef BSP_MOTOR_LEFT_INVERT
#define BSP_MOTOR_LEFT_INVERT 0
#endif
#ifndef BSP_MOTOR_RIGHT_INVERT
#define BSP_MOTOR_RIGHT_INVERT 0
#endif
#define BSP_ENCODER_LEFT_PORT GPIOC
#define BSP_ENCODER_LEFT_PINS (GPIO_PIN_6 | GPIO_PIN_7)
#define BSP_ENCODER_RIGHT_PORT GPIOD
#define BSP_ENCODER_RIGHT_PINS (GPIO_PIN_12 | GPIO_PIN_13)
#define BSP_SPI_PORT GPIOB
#define BSP_SPI_PINS (GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15)
#define BSP_IMU_CS_PORT GPIOB
#define BSP_IMU_CS_PIN GPIO_PIN_12
#define BSP_TRACK_CS_PORT GPIOE
#define BSP_TRACK_CS_PIN GPIO_PIN_7
#define BSP_IMU_INT_PORT GPIOB
#define BSP_IMU_INT_PIN GPIO_PIN_10
#define BSP_UART_PORT GPIOD
#define BSP_UART_PINS (GPIO_PIN_5 | GPIO_PIN_6)

extern TIM_HandleTypeDef htim1, htim3, htim4, htim6;
extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;

/* Minimal bring-up observability; no motion commands or sensor transactions. */
extern volatile uint32_t bsp_control_ticks;
extern volatile uint32_t bsp_imu_edges;
extern volatile uint32_t bsp_uart_rx_bytes;
extern volatile uint32_t bsp_uart_errors;

void Bsp_Init(void);
void Bsp_Clock_Config(void);
/* Low-level initialization/driver helper, requires TIM1 clock and PWM setup.
 * Application stops must use Motor_Stop so the software enable is revoked. */
void Bsp_MotorStop(void);
void Error_Handler(void);

#endif
