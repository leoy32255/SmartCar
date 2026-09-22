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
  *          【注意】stm32f1xx_it.c 里不要再定义 SysTick_Handler / EXTI0_IRQHandler /
  *          USART2_IRQHandler，否则与本工程的实现重复定义，链接会报错。
  ******************************************************************************
  */

#include "main.h"
#include "bsp_config.h"
#include "app.h"

/* ==========================================================================
 * 中断服务程序
 * ========================================================================== */

/**
 * @brief  SysTick 中断（HAL 时基 + 控制节拍）
 * @note   HAL_IncTick() 必须在最前面，它是 HAL_Delay / HAL_GetTick 的基础。
 *         控制节拍的置位由 App_Tick_1ms() 内部按 CTRL_PERIOD_MS 分频。
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
    App_Tick_1ms();
}

/* EXTI0_IRQHandler  -> imu.c（MPU6500 数据就绪）
 * USART2_IRQHandler -> comm.c（蓝牙收发）
 * 放在各自模块里，符合"外设中断由驱动层自己管"的分层原则。 */

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
     * 控制任务由 SysTick 置的标志触发，严格 5ms 一次。 */
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
