#include "bsp_config.h"

TIM_HandleTypeDef htim1, htim3, htim4, htim6;
SPI_HandleTypeDef hspi2;
UART_HandleTypeDef huart2;

_Static_assert(BSP_PWM_TIMER_HZ / (BSP_PWM_PSC + 1U) / (BSP_PWM_ARR + 1U) == 2000U,
               "Revision 3 requires 2 kHz PWM");
_Static_assert(BSP_TICK_TIMER_HZ / (BSP_TICK_PSC + 1U) / (BSP_TICK_ARR + 1U) ==
               1000U / BSP_CTRL_PERIOD_MS, "Control tick must be 5 ms");

static void check(HAL_StatusTypeDef status)
{
    if (status != HAL_OK) Error_Handler();
}

static void gpio(GPIO_TypeDef *port, uint32_t pins, uint32_t mode,
                 uint32_t pull, uint32_t alternate)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = pins;
    init.Mode = mode;
    init.Pull = pull;
    init.Speed = GPIO_SPEED_FREQ_HIGH;
    init.Alternate = alternate;
    HAL_GPIO_Init(port, &init);
}

/* This runs before waiting for HSE: preload ODR before enabling output drivers. */
static void safe_gpio_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_SYSCFG_CLK_ENABLE();

    HAL_GPIO_WritePin(BSP_MOTOR_PORT, BSP_MOTOR_DIRECTIONS, GPIO_PIN_SET);
    gpio(BSP_MOTOR_PORT, BSP_MOTOR_DIRECTIONS, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
    HAL_GPIO_WritePin(BSP_MOTOR_PORT, BSP_MOTOR_PWM_PINS, GPIO_PIN_RESET);
    gpio(BSP_MOTOR_PORT, BSP_MOTOR_PWM_PINS, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
    HAL_GPIO_WritePin(BSP_IMU_CS_PORT, BSP_IMU_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(BSP_TRACK_CS_PORT, BSP_TRACK_CS_PIN, GPIO_PIN_SET);
    gpio(BSP_IMU_CS_PORT, BSP_IMU_CS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
    gpio(BSP_TRACK_CS_PORT, BSP_TRACK_CS_PIN, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL, 0);
}

void Bsp_Clock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    /* F407 uses scale 1 at 168 MHz; OverDrive is not an F407 peripheral. */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM = 8;
    osc.PLL.PLLN = 336;
    osc.PLL.PLLP = RCC_PLLP_DIV2;
    osc.PLL.PLLQ = 7;
    check(HAL_RCC_OscConfig(&osc));
    clk.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    check(HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5));
    if (HAL_RCC_GetSysClockFreq() != BSP_SYSCLK_HZ ||
        HAL_RCC_GetPCLK1Freq() != BSP_APB1_HZ ||
        HAL_RCC_GetPCLK2Freq() != BSP_APB2_HZ) Error_Handler();
}

static void pwm_init(void)
{
    TIM_OC_InitTypeDef channel = {0};
    __HAL_RCC_TIM1_CLK_ENABLE();
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = BSP_PWM_PSC;
    htim1.Init.Period = BSP_PWM_ARR;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    check(HAL_TIM_PWM_Init(&htim1));
    channel.OCMode = TIM_OCMODE_PWM1;
    channel.Pulse = 0;
    channel.OCPolarity = TIM_OCPOLARITY_HIGH;
    channel.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    channel.OCIdleState = TIM_OCIDLESTATE_RESET;
    channel.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    channel.OCFastMode = TIM_OCFAST_DISABLE;
    check(HAL_TIM_PWM_ConfigChannel(&htim1, &channel, TIM_CHANNEL_1));
    check(HAL_TIM_PWM_ConfigChannel(&htim1, &channel, TIM_CHANNEL_2));
    check(HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1));
    check(HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2));
    /* Switch to AF only after the timer is producing a zero duty output. */
    gpio(BSP_MOTOR_PORT, BSP_MOTOR_PWM_PINS, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF1_TIM1);
}

static void encoder_init(TIM_HandleTypeDef *timer, TIM_TypeDef *instance)
{
    TIM_Encoder_InitTypeDef encoder = {0};
    timer->Instance = instance;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Period = 0xffffU;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    encoder.EncoderMode = TIM_ENCODERMODE_TI12;
    encoder.IC1Polarity = encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC1Selection = encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC1Prescaler = encoder.IC2Prescaler = TIM_ICPSC_DIV1;
    encoder.IC1Filter = encoder.IC2Filter = 10;
    check(HAL_TIM_Encoder_Init(timer, &encoder));
    __HAL_TIM_SET_COUNTER(timer, 0);
    check(HAL_TIM_Encoder_Start(timer, TIM_CHANNEL_ALL));
}

static void spi_init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    gpio(BSP_SPI_PORT, BSP_SPI_PINS, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF5_SPI2);
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 7;
    check(HAL_SPI_Init(&hspi2));
    /* No transactions in this target: both devices stay deselected. */
}

static void uart_init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();
    gpio(BSP_UART_PORT, BSP_UART_PINS, GPIO_MODE_AF_PP, GPIO_PULLUP, GPIO_AF7_USART2);
    huart2.Instance = USART2;
    huart2.Init.BaudRate = BSP_UART_BAUD;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    check(HAL_UART_Init(&huart2));
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    /* The minimal ISR drains RX only; it cannot interpret motion commands. */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

static void tick_init(void)
{
    __HAL_RCC_TIM6_CLK_ENABLE();
    htim6.Instance = TIM6;
    htim6.Init.Prescaler = BSP_TICK_PSC;
    htim6.Init.Period = BSP_TICK_ARR;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    check(HAL_TIM_Base_Init(&htim6));
    /* HAL init generates UG; discard it to avoid a spurious first tick. */
    __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
    check(HAL_TIM_Base_Start_IT(&htim6));
}

void Bsp_MotorStop(void)
{
    /* Direction 11 is coast on this board, unlike legacy TB6612 semantics. */
    HAL_GPIO_WritePin(BSP_MOTOR_PORT, BSP_MOTOR_DIRECTIONS, GPIO_PIN_SET);
    BSP_MOTOR_PWM_TIMER->CCR1 = 0;
    BSP_MOTOR_PWM_TIMER->CCR2 = 0;
    /* HAL enables OC preload. Flush BOTH zero compares while bridges are 11:
     * otherwise an old active duty could drive the new direction for a cycle.
     * No TIM1 update IRQ/DMA is enabled. New duty latches at the next wrap. */
    BSP_MOTOR_PWM_TIMER->EGR = TIM_EGR_UG;
}

void Bsp_Init(void)
{
    safe_gpio_init();
    Bsp_Clock_Config();
    pwm_init();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_TIM4_CLK_ENABLE();
    gpio(BSP_ENCODER_LEFT_PORT, BSP_ENCODER_LEFT_PINS, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF2_TIM3);
    gpio(BSP_ENCODER_RIGHT_PORT, BSP_ENCODER_RIGHT_PINS, GPIO_MODE_AF_PP, GPIO_NOPULL, GPIO_AF2_TIM4);
    encoder_init(&htim3, TIM3);
    encoder_init(&htim4, TIM4);
    spi_init();
    uart_init();
    gpio(BSP_IMU_INT_PORT, BSP_IMU_INT_PIN, GPIO_MODE_IT_RISING, GPIO_PULLDOWN, 0);
    __HAL_GPIO_EXTI_CLEAR_IT(BSP_IMU_INT_PIN);
    HAL_NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 3, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    tick_init();
    Bsp_MotorStop();
}
