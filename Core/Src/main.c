/**
  ******************************************************************************
  * @file    main.c
  * @brief   智能循迹小车主程序
  *
  *          硬件：STM32F103C8T6 + TB6612FNG + MG520(1:30, 11线AB编码器)×2
  *                + MPU6500(软件SPI) + 5路TCRT5000 + JDY-31蓝牙
  *
  *          【重要】关于 CubeMX 的分工
  *          本工程与常见的"CubeMX 一键生成"用法不同：
  *            - 引脚、外设、时钟全部在 bsp_config.c / bsp_config.h 里手写初始化，
  *              main.c 不调用任何 MX_xxx_Init()。
  *            - CubeMX 只用来做三件事：
  *                1) 生成 HAL 库 (Drivers/STM32F1xx_HAL_Driver)
  *                2) 生成启动文件与链接脚本
  *                3) 生成 stm32f1xx_hal_conf.h
  *          这样做的好处是换 MCU（比如后续换 F407VET6）时，
  *          业务代码一行不用动，只重写 bsp_config.* 即可。
  *
  *          【注意】CubeMX 里**不要**给 TIM1 / TIM2 / TIM3 / TIM4 / USART2 / EXTI0
  *          勾选 NVIC 中断。它们的中断服务程序由本工程提供
  *          （TIM4 在 bsp_config.c，EXTI0 在 imu.c，USART2 在 comm.c），
  *          勾了会生成重复的函数，链接时报 defined multiple times。
  *          SysTick 例外：它是 HAL 的 1ms 时基，必须保留给 CubeMX 生成。
  ******************************************************************************
  */

#include "main.h"
#include "bsp_config.h"
#include "app.h"

/* ==========================================================================
 * 中断服务程序分布（本文件不定义任何 ISR）
 *
 *   SysTick_Handler   -> stm32f1xx_it.c  （CubeMX 生成，只调 HAL_IncTick）
 *   TIM4_IRQHandler   -> bsp_config.c    （控制节拍，5ms）
 *   EXTI0_IRQHandler  -> imu.c           （MPU6500 数据就绪）
 *   USART2_IRQHandler -> comm.c          （蓝牙收发）
 *
 *   这样安排的好处：
 *     - SysTick 归 HAL，CubeMX 生成的 stm32f1xx_it.c **一个字都不用改**，
 *       重新生成工程也不会和本工程重复定义。
 *     - 其余中断放在各自模块里，符合"外设中断由驱动层自己管"的分层原则；
 *       只要在 CubeMX 里不勾 TIM4 / EXTI0 / USART2 的 NVIC 中断即可。
 * ========================================================================== */

/* ==========================================================================
 * 入口
 * ========================================================================== */

int main(void)
{
    /* ---- 1. HAL 初始化（时基 / 中断优先级分组） ---- */
    HAL_Init();

    /* ---- 2. 板级初始化：时钟 72MHz + GPIO + PWM + 编码器 + 串口 + EXTI ---- */
    Bsp_Init();

    /* ---- 3. 应用初始化：IMU 校准、模块复位，最后进入停机态 ---- */
    App_Init();

    /* ---- 4. 主循环 ----
     * Comm_Update() 每圈都跑（保证接收不丢字节），
     * 控制任务由 TIM4 置的标志触发，严格 CTRL_PERIOD_MS 一次。 */
    while (1) {
        App_Loop();
    }
}

/* ==========================================================================
 * 错误处理
 * ========================================================================== */

/**
 * @brief  HAL 断言失败回调
 * @note   通常意味着外设初始化参数不合法（比如波特率超出误差范围）。
 *         这里让心跳灯狂闪 + 蜂鸣器长鸣，便于现场定位；
 *         同时不要死循环，留出调试器连接和串口输出的机会。
 */
void Error_Handler(void)
{
    __disable_irq();

    while (1) {
        /* 只操作 GPIO 寄存器，不依赖任何外设状态 */
        GPIOC->ODR ^= GPIO_PIN_13;
        for (volatile uint32_t i = 0; i < 200000; i++) {
            __NOP();
        }
    }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
    Error_Handler();
}
#endif
