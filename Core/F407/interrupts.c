#include "bsp_config.h"
#include "uart_f407.h"

volatile uint32_t bsp_control_ticks;
volatile uint32_t bsp_imu_edges;
volatile uint32_t bsp_uart_rx_bytes;
volatile uint32_t bsp_uart_errors;

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void TIM6_DAC_IRQHandler(void)
{
    /* DAC is not enabled. TIM4 belongs exclusively to the right encoder. */
    if (__HAL_TIM_GET_FLAG(&htim6, TIM_FLAG_UPDATE) != RESET &&
        __HAL_TIM_GET_IT_SOURCE(&htim6, TIM_IT_UPDATE) != RESET) {
        __HAL_TIM_CLEAR_IT(&htim6, TIM_IT_UPDATE);
        ++bsp_control_ticks;
    }
}

void EXTI15_10_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(BSP_IMU_INT_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(BSP_IMU_INT_PIN);
        ++bsp_imu_edges;
    }
}

void USART2_IRQHandler(void)
{
    /* SR then DR clears RXNE/ORE/NE/FE/PE on STM32F407. */
    uint32_t status = USART2->SR;
    if (status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) {
        uint8_t byte=(uint8_t)USART2->DR;
        Uart_ReceiveISR(byte,HAL_GetTick(),(status & (USART_SR_ORE|USART_SR_NE|USART_SR_FE|USART_SR_PE))!=0);
        if (status & USART_SR_RXNE) ++bsp_uart_rx_bytes;
        if (status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)) ++bsp_uart_errors;
    }
    if((status & USART_SR_TXE) && (USART2->CR1 & USART_CR1_TXEIE)) Uart_TransmitISR();
}

void NMI_Handler(void) { Error_Handler(); }
void HardFault_Handler(void) { Error_Handler(); }
void MemManage_Handler(void) { Error_Handler(); }
void BusFault_Handler(void) { Error_Handler(); }
void UsageFault_Handler(void) { Error_Handler(); }
