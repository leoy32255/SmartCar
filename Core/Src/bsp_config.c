/**
  ******************************************************************************
  * @file    bsp_config.c
  * @brief   板级初始化实现：时钟、GPIO、PWM、编码器、串口、外部中断
  *
  *          换板子时，本文件与 bsp_config.h 是需要动的全部代码。
  ******************************************************************************
  */

#include "bsp_config.h"
#include "main.h"       /* Error_Handler */

/* ==========================================================================
 * 外设句柄定义（其他模块通过 bsp_config.h 里的 extern 声明引用）
 * ========================================================================== */
TIM_HandleTypeDef htim1;    /* 电机 PWM */
TIM_HandleTypeDef htim2;    /* 左编码器 */
TIM_HandleTypeDef htim3;    /* 右编码器 */
UART_HandleTypeDef huart2;  /* 蓝牙 */

/* ==========================================================================
 * 1. 时钟树
 *
 *    F103C8T6: HSE 8MHz -> PLL×9  -> SYSCLK  72MHz
 *              AHB 72 / APB1 36 / APB2 72   (TIM1 时钟 72MHz)
 *
 *    F407VET6: HSE 8MHz -> PLLM=8, PLLN=336, PLLP=2, PLLQ=7 -> SYSCLK 168MHz
 *              AHB 168 / APB1 42 / APB2 84  (TIM1 时钟 168MHz)
 *
 *    注意：APB1 分频 ≠ 1 时，挂在 APB1 上的定时器时钟 = APB1 × 2。
 *    F1: 36×2 = 72MHz；F4: 42×2 = 84MHz。编码器只数边沿，
 *    时基变化不影响计数结果，所以 ENCODER_CPR 不用改。
 * ========================================================================== */
void Bsp_Clock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

#if defined(STM32F103xB) || defined(STM32F103xE)
    /* ---------------- F1: 72MHz ---------------- */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState            = RCC_HSE_ON;
    osc.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    osc.HSIState            = RCC_HSI_ON;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL          = RCC_PLL_MUL9;         /* 8MHz × 9 = 72MHz */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();                            /* 晶振起振失败 */
    }

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;           /* 72MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV2;             /* 36MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV1;             /* 72MHz */
    if (HAL_RCC_ClockConfig(&clk, BSP_FLASH_LATENCY) != HAL_OK) {
        Error_Handler();
    }

#elif defined(STM32F407xx)
    /* ---------------- F4: 168MHz ----------------
     * 首先使能 FPU（CP10/CP11 全访问）。
     * Makefile 在 F4 下用 -mfloat-abi=hard 编译，此时编译器会把
     * 普通的 float 运算编成 FPU 指令；如果 CPACR 没开，第一条
     * 浮点指令就会触发 HardFault，而且现象是"进 main 就死"，
     * 很难定位。ST 的启动文件里 SystemInit() 也会做这件事，
     * 但这里显式再做一次，避免启动文件被替换/裁剪后留下隐患。
     */
#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
    SCB->CPACR |= ((3UL << (10U * 2U)) | (3UL << (11U * 2U)));
#endif

    /* F4 的 PLL 结构是 M/N/P/Q，和 F1 的"单倍频系数"完全不同。
     * VCO = HSE / PLLM × PLLN = 8 / 8 × 336 = 336MHz
     * SYSCLK = VCO / PLLP = 336 / 2 = 168MHz
     * 48MHz 域（USB/SDIO）用 PLLQ = 7 -> 336/7 = 48MHz
     * PLLM 必须让 VCO 输入落在 1~2MHz，推荐 2MHz：8/8 = 1MHz 也可，
     * 这里取 8 使 VCO 输入为 1MHz（规格允许 1~2MHz，且抖动更小）。 */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = 8;
    osc.PLL.PLLN       = 336;
    osc.PLL.PLLP       = RCC_PLLP_DIV2;
    osc.PLL.PLLQ       = 7;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        Error_Handler();
    }

    /* F4 跑 168MHz 前必须先设置调压器等级与过驱动 */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    HAL_PWREx_EnableOverDrive();

    clk.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                       | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;           /* 168MHz */
    clk.APB1CLKDivider = RCC_HCLK_DIV4;             /* 42MHz */
    clk.APB2CLKDivider = RCC_HCLK_DIV2;             /* 84MHz */
    if (HAL_RCC_ClockConfig(&clk, BSP_FLASH_LATENCY) != HAL_OK) {
        Error_Handler();
    }
#endif
}

/* ==========================================================================
 * 2. GPIO 静态辅助函数
 * ========================================================================== */
static void gpio_out_pp(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = pin;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &g);
}

static void gpio_in_pullup(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(port, &g);
}

static void gpio_in_floating(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(port, &g);
}

/**
 * @brief  配置为复用推挽输出
 * @param  af  复用功能编号。F1 上该参数被忽略（F1 靠 AFIO 重映射，
 *             HAL 结构体里没有 Alternate 字段）；
 *             F4 必须传正确的 GPIO_AFx_xxx，否则引脚不会有输出。
 */
static void gpio_af_pp(GPIO_TypeDef *port, uint16_t pin, uint32_t af)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = pin;
    g.Mode  = GPIO_MODE_AF_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
#if BSP_GPIO_HAS_AF
    g.Alternate = af;
#else
    (void)af;
#endif
    HAL_GPIO_Init(port, &g);
}

/* ==========================================================================
 * 3. GPIO 初始化
 * ========================================================================== */
static void Bsp_Gpio_Init(void)
{
    /* 使能所有用到的端口时钟 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* EXTI 线映射所需的时钟：F1 是 AFIO，F4 是 SYSCFG */
    BSP_EXTI_CLK_ENABLE();

    /* ---- 电机方向 + STBY（TB6612） ---- */
    gpio_out_pp(MOTOR_AIN1_PORT, MOTOR_AIN1_PIN);
    gpio_out_pp(MOTOR_AIN2_PORT, MOTOR_AIN2_PIN);
    gpio_out_pp(MOTOR_BIN1_PORT, MOTOR_BIN1_PIN);
    gpio_out_pp(MOTOR_BIN2_PORT, MOTOR_BIN2_PIN);
    gpio_out_pp(MOTOR_STBY_PORT, MOTOR_STBY_PIN);

    /* 上电先让 TB6612 处于关断态，避免初始化过程中电机乱转 */
#if MOTOR_STBY_ACTIVE_HIGH
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(MOTOR_STBY_PORT, MOTOR_STBY_PIN, GPIO_PIN_SET);
#endif

    /* ---- 编码器 A/B 相 ----
     * F1：编码器输入配浮空输入即可，不需要 AF。
     * F4：定时器输入通道必须显式配置 AF，否则 TIM2/TIM3 数不到脉冲，
     *     现象是"速度一直是 0"，很容易误判成编码器坏了。
     *     这是 F1 -> F4 移植最容易踩的坑之一。 */
#if BSP_GPIO_HAS_AF
    gpio_af_pp(GPIOA, GPIO_PIN_0, BSP_AF_TIM2);     /* TIM2_CH1 左 A */
    gpio_af_pp(GPIOA, GPIO_PIN_1, BSP_AF_TIM2);     /* TIM2_CH2 左 B */
    gpio_af_pp(GPIOA, GPIO_PIN_6, BSP_AF_TIM3);     /* TIM3_CH1 右 A */
    gpio_af_pp(GPIOA, GPIO_PIN_7, BSP_AF_TIM3);     /* TIM3_CH2 右 B */
#else
    gpio_in_floating(GPIOA, GPIO_PIN_0);            /* TIM2_CH1 左 A */
    gpio_in_floating(GPIOA, GPIO_PIN_1);            /* TIM2_CH2 左 B */
    gpio_in_floating(GPIOA, GPIO_PIN_6);            /* TIM3_CH1 右 A */
    gpio_in_floating(GPIOA, GPIO_PIN_7);            /* TIM3_CH2 右 B */
#endif

    /* ---- 电机 PWM：PA8 / PA9 复用推挽 ---- */
    gpio_af_pp(GPIOA, GPIO_PIN_8, BSP_AF_TIM1);     /* TIM1_CH1 */
    gpio_af_pp(GPIOA, GPIO_PIN_9, BSP_AF_TIM1);     /* TIM1_CH2 */

    /* ---- MPU6500 软件 SPI ---- */
    gpio_out_pp(IMU_CS_PORT,   IMU_CS_PIN);
    gpio_out_pp(IMU_SCK_PORT,  IMU_SCK_PIN);
    gpio_out_pp(IMU_MOSI_PORT, IMU_MOSI_PIN);
    gpio_in_pullup(IMU_MISO_PORT, IMU_MISO_PIN);
    HAL_GPIO_WritePin(IMU_CS_PORT, IMU_CS_PIN, GPIO_PIN_SET);   /* CS 空闲拉高 */
    HAL_GPIO_WritePin(IMU_SCK_PORT, IMU_SCK_PIN, GPIO_PIN_SET); /* 模式 3：SCLK 空闲高 */

    /* ---- 循迹 5 路上拉输入（上拉保证悬空时读到"未检测"） ---- */
    gpio_in_pullup(TRACK1_PORT, TRACK1_PIN);
    gpio_in_pullup(TRACK2_PORT, TRACK2_PIN);
    gpio_in_pullup(TRACK3_PORT, TRACK3_PIN);
    gpio_in_pullup(TRACK4_PORT, TRACK4_PIN);
    gpio_in_pullup(TRACK5_PORT, TRACK5_PIN);

    /* ---- 串口 PA2=TX 复用推挽, PA3=RX 输入浮空 ----
     * F4 上 RX 也需要配成 AF 模式（GPIO_AF7_USART2），
     * 不能像 F1 那样留作普通浮空输入，否则收不到数据。 */
#if BSP_GPIO_HAS_AF
    gpio_af_pp(GPIOA, GPIO_PIN_2, BSP_AF_USART2);
    gpio_af_pp(GPIOA, GPIO_PIN_3, BSP_AF_USART2);
#else
    gpio_af_pp(GPIOA, GPIO_PIN_2, BSP_AF_USART2);
    gpio_in_floating(GPIOA, GPIO_PIN_3);
#endif

    /* ---- 蜂鸣器 / 指示灯 ---- */
    gpio_out_pp(BUZZER_PORT, BUZZER_PIN);
#if BUZZER_ACTIVE_HIGH
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
#endif

    /* PC13 板载 LED：开漏输出 */
    {
        GPIO_InitTypeDef g = {0};
        g.Pin   = LED_PIN;
        g.Mode  = GPIO_MODE_OUTPUT_OD;
        g.Pull  = GPIO_NOPULL;
        g.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(LED_PORT, &g);
    }
#if LED_ACTIVE_HIGH
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
#endif

    /* ---- MPU6500 INT (PB0) 外部中断，上升沿触发 ---- */
    gpio_in_floating(IMU_INT_PORT, IMU_INT_PIN);
    {
        GPIO_InitTypeDef g = {0};
        g.Pin  = IMU_INT_PIN;
        g.Mode = GPIO_MODE_IT_RISING;
        g.Pull = GPIO_PULLDOWN;     /* INT 空闲低；用下拉避免悬空误触发 */
        HAL_GPIO_Init(IMU_INT_PORT, &g);
    }

    /* EXTI0 优先级：低于串口，高于 SysTick */
    HAL_NVIC_SetPriority(IMU_INT_EXTI_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(IMU_INT_EXTI_IRQn);
}

/* ==========================================================================
 * 4. 电机 PWM：TIM1 CH1/CH2 @ 20kHz
 *
 *    ARR 由实际定时器时钟算出，不写死：
 *      F1: 72MHz  / 20000 - 1 = 3599
 *      F4: 168MHz / 20000 - 1 = 8399
 *    这样换 MCU 后 PWM 频率自动保持 20kHz，不用手改常数。
 * ========================================================================== */
static void Bsp_Pwm_Init(void)
{
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_TIM1_CLK_ENABLE();

    htim1.Instance               = TIM1;
    htim1.Init.Prescaler         = 0;
    htim1.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim1.Init.Period            = (BSP_TIM1_CLK_HZ / MOTOR_PWM_FREQ_HZ) - 1U;
    htim1.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        while (1) { }
    }

    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, MOTOR_PWM_CH_LEFT) != HAL_OK)  { while (1) { } }
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, MOTOR_PWM_CH_RIGHT) != HAL_OK) { while (1) { } }

    /* TIM1 是高级定时器，HAL_TIM_PWM_Start 内部会置 BDTR.MOE 使能输出 */
    HAL_TIM_PWM_Start(&htim1, MOTOR_PWM_CH_LEFT);
    HAL_TIM_PWM_Start(&htim1, MOTOR_PWM_CH_RIGHT);

    __HAL_TIM_SET_COMPARE(&htim1, MOTOR_PWM_CH_LEFT,  0);
    __HAL_TIM_SET_COMPARE(&htim1, MOTOR_PWM_CH_RIGHT, 0);
}

/* ==========================================================================
 * 5. 编码器：TIM2 / TIM3 正交编码器模式（4 倍频）
 * ========================================================================== */
static void Bsp_Encoder_Timer_Init(TIM_HandleTypeDef *htim, TIM_TypeDef *inst)
{
    TIM_Encoder_InitTypeDef enc = {0};

    htim->Instance           = inst;
    htim->Init.Prescaler     = 0;                       /* 每个边沿都计数，不分频 */
    htim->Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim->Init.Period        = 0xFFFF;                  /* 16 位满量程，软件处理回绕 */
    htim->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    /* TI12：CH1/CH2 都参与计数，上下边沿都触发 -> 4 倍频 */
    enc.EncoderMode  = TIM_ENCODERMODE_TI12;
    enc.IC1Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC1Prescaler = TIM_ICPSC_DIV1;
    enc.IC1Filter    = 10;      /* 输入滤波 10 个采样周期，抑制 MG520 长线毛刺 */
    enc.IC2Polarity  = TIM_ICPOLARITY_RISING;
    enc.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    enc.IC2Prescaler = TIM_ICPSC_DIV1;
    enc.IC2Filter    = 10;

    if (HAL_TIM_Encoder_Init(htim, &enc) != HAL_OK) {
        while (1) { }
    }
    HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
    __HAL_TIM_SET_COUNTER(htim, 0);
}

static void Bsp_Encoder_Init(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    Bsp_Encoder_Timer_Init(&htim2, TIM2);   /* 左轮，PA0/PA1 */
    Bsp_Encoder_Timer_Init(&htim3, TIM3);   /* 右轮，PA6/PA7 */
}

/* ==========================================================================
 * 6. 串口 USART2（蓝牙 JDY-31）
 * ========================================================================== */
static void Bsp_Uart_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance          = USART2;
    huart2.Init.BaudRate     = COMM_BAUDRATE;
    huart2.Init.WordLength   = UART_WORDLENGTH_8B;
    huart2.Init.StopBits     = UART_STOPBITS_1;
    huart2.Init.Parity       = UART_PARITY_NONE;
    huart2.Init.Mode         = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        while (1) { }
    }

    /* 波特率偏差异常时 HAL_UART_Init 内部已经做了校验，
     * 这里再打一次标记，方便上电用示波器/逻辑分析仪直接看 TX 波形。 */

    HAL_NVIC_SetPriority(COMM_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(COMM_IRQn);

    /* 逐字节中断接收：由 comm.c 的 HAL_UART_Receive_IT 循环挂载 */
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}

/* ==========================================================================
 * 7. 总入口
 * ========================================================================== */
void Bsp_Init(void)
{
    Bsp_Clock_Config();

    /* HAL_Init() 已在 main() 最先调用，此处只需保证 SysTick 为 1ms 基准 */
    HAL_SYSTICK_Config(SystemCoreClock / 1000U);

    Bsp_Gpio_Init();
    Bsp_Pwm_Init();
    Bsp_Encoder_Init();
    Bsp_Uart_Init();
}
