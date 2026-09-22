/**
  ******************************************************************************
  * @file    imu.c
  * @brief   MPU6500 软件 SPI 驱动实现
  *
  *          SPI 时序（模式 3：CPOL=1, CPHA=1）
  *            - SCLK 空闲为高
  *            - 下降沿输出数据位，上升沿采样
  *          位操作顺序：
  *            拉低 SCLK -> 摆好 MOSI -> 延时 -> 拉高 SCLK(从机采样) -> 读 MISO -> 延时
  ******************************************************************************
  */

#include "imu.h"

/* ---- 原始数据缓存 ------------------------------------------------------ */
static int16_t s_acc[3]  = {0};
static int16_t s_gyro[3] = {0};
static int16_t s_temp    = 0;

/* ---- 零偏 (°/s) -------------------------------------------------------- */
static float   s_gyro_bias[3] = {0.0f};

/* ---- 状态 -------------------------------------------------------------- */
static volatile uint8_t s_data_ready = 0;   /* EXTI 中断置位 */
static uint8_t  s_dev_id   = 0;
static uint8_t  s_online   = 0;

/* ==========================================================================
 * 软件 SPI 底层
 * ========================================================================== */

#define IMU_CS_LOW()    HAL_GPIO_WritePin(IMU_CS_PORT,  IMU_CS_PIN,  GPIO_PIN_RESET)
#define IMU_CS_HIGH()   HAL_GPIO_WritePin(IMU_CS_PORT,  IMU_CS_PIN,  GPIO_PIN_SET)
#define IMU_SCK_LOW()   HAL_GPIO_WritePin(IMU_SCK_PORT, IMU_SCK_PIN, GPIO_PIN_RESET)
#define IMU_SCK_HIGH()  HAL_GPIO_WritePin(IMU_SCK_PORT, IMU_SCK_PIN, GPIO_PIN_SET)
#define IMU_MOSI_LOW()  HAL_GPIO_WritePin(IMU_MOSI_PORT,IMU_MOSI_PIN,GPIO_PIN_RESET)
#define IMU_MOSI_HIGH() HAL_GPIO_WritePin(IMU_MOSI_PORT,IMU_MOSI_PIN,GPIO_PIN_SET)
#define IMU_MISO_READ() HAL_GPIO_ReadPin(IMU_MISO_PORT, IMU_MISO_PIN)

/** 半周期延时。72MHz 下每次循环约 3~4 周期，2 次 ≈ 0.1μs -> SCLK ≈ 3MHz。
 *  杜邦线较长时若读数不稳，把 IMU_SPI_DELAY_LOOPS 加到 4~8 降速即可。 */
static inline void spi_delay(void)
{
    for (volatile uint32_t i = 0; i < IMU_SPI_DELAY_LOOPS; i++) {
        __NOP();
    }
}

/** 全双工交换一个字节，返回从机在本次时钟中送出的数据 */
static uint8_t spi_transfer(uint8_t tx)
{
    uint8_t rx = 0;

    for (uint8_t i = 0; i < 8; i++) {
        IMU_SCK_LOW();                      /* 下降沿：主机摆数据 */

        if (tx & 0x80) {
            IMU_MOSI_HIGH();
        } else {
            IMU_MOSI_LOW();
        }
        tx <<= 1;
        spi_delay();

        IMU_SCK_HIGH();                     /* 上升沿：从机采样，同时从机输出 */
        spi_delay();

        rx <<= 1;
        if (IMU_MISO_READ() == GPIO_PIN_SET) {
            rx |= 0x01;
        }
    }

    IMU_SCK_HIGH();     /* 保持空闲高电平（模式 3） */
    return rx;
}

/** 写寄存器 */
static void mpu_write_reg(uint8_t reg, uint8_t val)
{
    IMU_CS_LOW();
    spi_delay();
    spi_transfer(reg & 0x7F);       /* bit7=0 -> 写 */
    spi_transfer(val);
    spi_delay();
    IMU_CS_HIGH();
}

/** 读寄存器 */
static uint8_t mpu_read_reg(uint8_t reg)
{
    uint8_t val;

    IMU_CS_LOW();
    spi_delay();
    spi_transfer(reg | 0x80);       /* bit7=1 -> 读 */
    val = spi_transfer(0x00);
    spi_delay();
    IMU_CS_HIGH();

    return val;
}

/** 连续读多个寄存器（自动递增地址，用于 14 字节突发读） */
static void mpu_read_burst(uint8_t reg, uint8_t *buf, uint8_t len)
{
    IMU_CS_LOW();
    spi_delay();
    spi_transfer(reg | 0x80);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = spi_transfer(0x00);
    }
    spi_delay();
    IMU_CS_HIGH();
}

/* ==========================================================================
 * 初始化
 * ========================================================================== */

uint8_t Imu_Init(void)
{
    uint8_t id;
    uint8_t who;

    s_online = 0;

    /* 上电稳定时间：模块内部有低压差稳压，等 100ms */
    HAL_Delay(100);

    /* --- 1. 复位器件 --- */
    mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x80);    /* DEVICE_RESET */
    HAL_Delay(100);

    /* --- 2. 关闭 I2C 接口，切到 SPI 模式（关键步骤） --- */
    mpu_write_reg(MPU_REG_USER_CTRL, MPU_USERCTRL_I2C_IF_DIS);
    HAL_Delay(10);

    /* --- 3. 唤醒，时钟源选 X 轴陀螺 PLL（比内部 RC 稳得多） --- */
    mpu_write_reg(MPU_REG_PWR_MGMT_1, 0x01);
    HAL_Delay(10);
    mpu_write_reg(MPU_REG_PWR_MGMT_2, 0x00);

    /* --- 4. 校验器件 ID --- */
    who = mpu_read_reg(MPU_REG_WHO_AM_I);
    s_dev_id = who;

    id = who & 0x7F;    /* 部分批次最高位有保留位 */
    if (id != 0x70 && id != 0x71 && id != 0x73 && id != 0x68) {
        return 1;       /* 不是 MPU 系列，或 SPI 接线/片选有问题 */
    }

    /* --- 5. 采样率与滤波 --- */
    mpu_write_reg(MPU_REG_CONFIG, 0x03);            /* DLPF_CFG=3: 陀螺 42Hz / 加计 44Hz */
    mpu_write_reg(MPU_REG_SMPLRT_DIV, IMU_SMPLRT_DIV);  /* 1kHz/(1+4) = 200Hz */

    /* --- 6. 量程 --- */
    mpu_write_reg(MPU_REG_GYRO_CONFIG,  (uint8_t)(IMU_GYRO_FS_SEL  << 3));
    mpu_write_reg(MPU_REG_ACCEL_CONFIG, (uint8_t)(IMU_ACCEL_FS_SEL << 3));

    /* --- 7. 中断引脚：推挽、高有效、锁存到读 INT_STATUS 才释放 ---
     * 锁存很重要：数据就绪脉冲只有 50μs，若不锁存，
     * 恰好落在中断服务程序执行期间的那次触发就会丢，导致采样率不稳。 */
    mpu_write_reg(MPU_REG_INT_PIN_CFG, 0x20);       /* LATCH_INT_EN=1 */
    mpu_write_reg(MPU_REG_INT_ENABLE,  0x01);       /* DATA_RDY_EN */

    /* 清一次中断状态，避免残留标志立刻触发 EXTI */
    (void)mpu_read_reg(MPU_REG_INT_STATUS);

    s_online = 1;
    return 0;
}

/* ==========================================================================
 * 数据读取
 * ========================================================================== */

void Imu_ReadData(void)
{
    uint8_t buf[14];

    if (!s_online) {
        return;
    }

    /* 0x3B 起连续 14 字节：ACC[6] TEMP[2] GYRO[6] */
    mpu_read_burst(MPU_REG_ACCEL_XOUT_H, buf, 14);

    s_acc[0]  = (int16_t)((buf[0]  << 8) | buf[1]);
    s_acc[1]  = (int16_t)((buf[2]  << 8) | buf[3]);
    s_acc[2]  = (int16_t)((buf[4]  << 8) | buf[5]);
    s_temp    = (int16_t)((buf[6]  << 8) | buf[7]);
    s_gyro[0] = (int16_t)((buf[8]  << 8) | buf[9]);
    s_gyro[1] = (int16_t)((buf[10] << 8) | buf[11]);
    s_gyro[2] = (int16_t)((buf[12] << 8) | buf[13]);

    /* 读 INT_STATUS 释放锁存的中断引脚，否则 PB0 会一直保持高电平 */
    if (s_data_ready) {
        (void)mpu_read_reg(MPU_REG_INT_STATUS);
        s_data_ready = 0;
    }
}

uint8_t Imu_DataReady(void)     { return s_data_ready; }

int16_t Imu_GetAccelX(void)     { return s_acc[0]; }
int16_t Imu_GetAccelY(void)     { return s_acc[1]; }
int16_t Imu_GetAccelZ(void)     { return s_acc[2]; }
int16_t Imu_GetGyroX(void)      { return s_gyro[0]; }
int16_t Imu_GetGyroY(void)      { return s_gyro[1]; }
int16_t Imu_GetGyroZ(void)      { return s_gyro[2]; }

float Imu_GetAccelG(uint8_t axis)
{
    if (axis > 2) return 0.0f;
    return (float)s_acc[axis] / IMU_ACCEL_LSB_PER_G;
}

float Imu_GetGyroDps(uint8_t axis)
{
    if (axis > 2) return 0.0f;
    /* 减去零偏，滤波层拿到的就是"净角速度" */
    return ((float)s_gyro[axis] / IMU_GYRO_LSB_PER_DPS) - s_gyro_bias[axis];
}

float Imu_GetGyroBias(uint8_t axis)
{
    if (axis > 2) return 0.0f;
    return s_gyro_bias[axis];
}

uint8_t Imu_GetDeviceID(void)   { return s_dev_id; }

/* ==========================================================================
 * 零偏校准
 * ========================================================================== */

uint8_t Imu_CalibrateGyro(uint16_t samples)
{
    float    sum[3] = {0.0f, 0.0f, 0.0f};
    int32_t  maxv[3], minv[3];
    uint16_t i;

    if (!s_online || samples == 0) {
        return 1;
    }

    /* 取第一组作为极值初值，先读一次保证有数据。
     * 带超时：如果 IMU 的 INT 引脚没接好 / EXTI 没配置，
     * 这里会一直等下去。超时后主动读一次（SPI 是主机发起的，
     * 不依赖 INT），保证即使 INT 断了也能完成校准。 */
    {
        uint32_t timeout = 100000;
        while (!Imu_DataReady() && timeout) {
            timeout--;
        }
        Imu_ReadData();         /* 无论是否有 INT 都读一次 */
    }

    for (uint8_t a = 0; a < 3; a++) {
        maxv[a] = minv[a] = s_gyro[a];
    }

    for (i = 0; i < samples; i++) {
        /* 等数据就绪，超时则用当前值继续（避免 IMU 掉线时死等） */
        uint32_t timeout = 10000;
        while (!Imu_DataReady() && timeout) { timeout--; }
        Imu_ReadData();

        for (uint8_t a = 0; a < 3; a++) {
            sum[a] += (float)s_gyro[a];
            if (s_gyro[a] > maxv[a]) maxv[a] = s_gyro[a];
            if (s_gyro[a] < minv[a]) minv[a] = s_gyro[a];
        }
    }

    /* 检查是否在校准过程中被移动：
     * ±2000dps 量程下 16.4 LSB/°/s，静止时峰峰值一般 < 300 LSB。
     * 超过 1500 LSB (≈90°/s) 认为车在动，校准结果不可信。 */
    for (uint8_t a = 0; a < 3; a++) {
        if ((maxv[a] - minv[a]) > 1500) {
            return 1;
        }
    }

    for (uint8_t a = 0; a < 3; a++) {
        s_gyro_bias[a] = (sum[a] / (float)samples) / IMU_GYRO_LSB_PER_DPS;
    }

    return 0;
}

/* ==========================================================================
 * 外部中断：数据就绪
 * ========================================================================== */

/**
 * @brief  MPU6500 INT -> PB0 -> EXTI0 上升沿
 * @note   中断里只置标志，SPI 读取放到主循环（5ms 节拍）里做。
 *         软件 SPI 一次 14 字节约 40μs，全速 200Hz 也才占 0.8% CPU，
 *         没必要把整数运算和位操作塞进中断上下文。
 */
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(IMU_INT_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(IMU_INT_PIN);
        s_data_ready = 1;
    }
}
