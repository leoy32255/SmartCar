/* Execute the real BSP against a host HAL boundary. No real peripheral access.
 * Peripheral layouts/constants come from the vendored ST/CMSIS headers.
 */
#ifdef _WIN32
#include <windows.h>
#undef ERROR
#else
#include <sys/mman.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bsp_config.h"

static GPIO_InitTypeDef pins[5][16];
static unsigned enabled[96];
static unsigned priorities[96];
static unsigned pwm_started;
static unsigned encoders_started;
static RCC_OscInitTypeDef oscillator;
static RCC_ClkInitTypeDef clocks;

void Error_Handler(void) { fputs("Unexpected BSP failure\n", stderr); abort(); }

static unsigned port_index(GPIO_TypeDef *port)
{
    return ((uintptr_t)port - (uintptr_t)GPIOA) / 0x400U;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t mask, GPIO_PinState state)
{
    if (state == GPIO_PIN_SET) port->ODR |= mask;
    else port->ODR &= ~mask;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *init)
{
    /* Check the externally visible electrical state when a pin starts driving. */
    if (port == GPIOE && init->Mode == GPIO_MODE_OUTPUT_PP) {
        if (init->Pin & 0x3500U) assert((port->ODR & (init->Pin & 0x3500U)) == (init->Pin & 0x3500U));
        if (init->Pin & 0x0a00U) assert((port->ODR & (init->Pin & 0x0a00U)) == 0);
    }
    if (port == GPIOE && init->Mode == GPIO_MODE_AF_PP && (init->Pin & 0x0a00U)) {
        assert(pwm_started == 3);
        assert(TIM1->CCR1 == 0 && TIM1->CCR2 == 0);
    }
    for (unsigned bit = 0; bit < 16; ++bit)
        if (init->Pin & (1U << bit)) pins[port_index(port)][bit] = *init;
}

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *config)
{
    /* Safe outputs must already exist even if HSE takes time to start. */
    assert((GPIOE->ODR & 0x3500U) == 0x3500U);
    assert((GPIOE->ODR & 0x0a00U) == 0);
    assert((PWR->CR & PWR_CR_VOS) == PWR_REGULATOR_VOLTAGE_SCALE1);
    oscillator = *config;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *config, uint32_t latency)
{
    assert(latency == FLASH_LATENCY_5);
    clocks = *config;
    return HAL_OK;
}
uint32_t HAL_RCC_GetSysClockFreq(void) { return 168000000U; }
uint32_t HAL_RCC_GetPCLK1Freq(void) { return 42000000U; }
uint32_t HAL_RCC_GetPCLK2Freq(void) { return 84000000U; }

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *timer)
{
    assert(timer->Instance == TIM1);
    timer->Instance->PSC = timer->Init.Prescaler;
    timer->Instance->ARR = timer->Init.Period;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer, const TIM_OC_InitTypeDef *config, uint32_t channel)
{
    assert(config->OCMode == TIM_OCMODE_PWM1 && config->OCPolarity == TIM_OCPOLARITY_HIGH);
    if (channel == TIM_CHANNEL_1) timer->Instance->CCR1 = config->Pulse;
    else if (channel == TIM_CHANNEL_2) timer->Instance->CCR2 = config->Pulse;
    else abort();
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
    assert(timer->Instance == TIM1);
    pwm_started |= channel == TIM_CHANNEL_1 ? 1U : 2U;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *timer, const TIM_Encoder_InitTypeDef *config)
{
    assert(timer->Instance == TIM3 || timer->Instance == TIM4);
    assert(config->EncoderMode == TIM_ENCODERMODE_TI12);
    assert(timer->Init.Prescaler == 0 && timer->Init.Period == 65535);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
    assert(channel == TIM_CHANNEL_ALL);
    encoders_started |= timer->Instance == TIM3 ? 1U : 2U;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *timer)
{
    assert(timer->Instance == TIM6);
    timer->Instance->SR = TIM_FLAG_UPDATE; /* HAL init generates UG */
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer)
{
    assert(!(timer->Instance->SR & TIM_FLAG_UPDATE));
    timer->Instance->DIER |= TIM_IT_UPDATE;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_SPI_Init(SPI_HandleTypeDef *spi)
{
    assert(spi->Instance == SPI2);
    assert(GPIOB->ODR & GPIO_PIN_12);
    assert(GPIOE->ODR & GPIO_PIN_7);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_Init(UART_HandleTypeDef *uart)
{
    assert(uart->Instance == USART2);
    return HAL_OK;
}
void HAL_NVIC_SetPriority(IRQn_Type irq, uint32_t priority, uint32_t sub)
{
    assert(sub == 0);
    priorities[irq] = priority;
}
void HAL_NVIC_EnableIRQ(IRQn_Type irq) { enabled[irq] = 1; }
void HAL_NVIC_ClearPendingIRQ(IRQn_Type irq) { (void)irq; }

static void check_af(unsigned port, unsigned pin, unsigned af)
{
    assert(pins[port][pin].Mode == GPIO_MODE_AF_PP);
    assert(pins[port][pin].Alternate == af);
}

int main(void)
{
    void *base = (void *)(uintptr_t)0x40000000;
#ifdef _WIN32
    void *memory = VirtualAlloc(base, 0x30000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void *memory = mmap(base, 0x30000, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
#endif
    assert(memory == base);
    memset(memory, 0, 0x30000);
    Bsp_Init();
    assert(oscillator.HSEState == RCC_HSE_ON && oscillator.PLL.PLLSource == RCC_PLLSOURCE_HSE);
    assert(oscillator.PLL.PLLM == 8 && oscillator.PLL.PLLN == 336);
    assert(oscillator.PLL.PLLP == RCC_PLLP_DIV2 && oscillator.PLL.PLLQ == 7);
    assert(clocks.SYSCLKSource == RCC_SYSCLKSOURCE_PLLCLK);
    assert(clocks.AHBCLKDivider == RCC_SYSCLK_DIV1);
    assert(clocks.APB1CLKDivider == RCC_HCLK_DIV4 && clocks.APB2CLKDivider == RCC_HCLK_DIV2);
    assert(htim1.Init.Prescaler == 83 && htim1.Init.Period == 999);
    assert(htim6.Init.Prescaler == 8399 && htim6.Init.Period == 49);
    assert(encoders_started == 3 && pwm_started == 3);
    check_af(4, 9, 1); check_af(4, 11, 1);
    check_af(2, 6, 2); check_af(2, 7, 2);
    check_af(3, 12, 2); check_af(3, 13, 2);
    check_af(1, 13, 5); check_af(1, 14, 5); check_af(1, 15, 5);
    check_af(3, 5, 7); check_af(3, 6, 7);
    assert(pins[1][10].Mode == GPIO_MODE_IT_RISING);
    assert(hspi2.Init.CLKPolarity == SPI_POLARITY_LOW && hspi2.Init.CLKPhase == SPI_PHASE_1EDGE);
    assert(hspi2.Init.BaudRatePrescaler == SPI_BAUDRATEPRESCALER_64);
    assert(huart2.Init.BaudRate == 9600 && huart2.Init.StopBits == UART_STOPBITS_1);
    assert(huart2.Init.WordLength == UART_WORDLENGTH_8B && huart2.Init.Parity == UART_PARITY_NONE);
    assert(enabled[TIM6_DAC_IRQn] && enabled[USART2_IRQn] && enabled[EXTI15_10_IRQn]);
    assert(!enabled[TIM4_IRQn] && !enabled[TIM3_IRQn] && !enabled[EXTI0_IRQn]);
    assert(priorities[TIM6_DAC_IRQn] < priorities[USART2_IRQn]);
    /* All GPIOA pins (including button, LED, USB and SWD) remain untouched. */
    for (unsigned pin = 0; pin < 16; ++pin) assert(pins[0][pin].Pin == 0);
    TIM1->CCR1 = 500; TIM1->CCR2 = 900; GPIOE->ODR &= ~0x3500U;
    Bsp_MotorStop();
    assert(TIM1->CCR1 == 0 && TIM1->CCR2 == 0);
    assert((GPIOE->ODR & 0x3500U) == 0x3500U);
    puts("BSP host boundary checks passed (no hardware execution)");
    return 0;
}
