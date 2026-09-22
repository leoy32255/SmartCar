/**
  ******************************************************************************
  * @file    imu.h
  * @brief   MPU6500 六轴姿态模块驱动（软件 SPI，模式 3）
  *
  *          为什么用软件 SPI 而不是硬件 I2C：
  *            - 本项目 5 路循迹 + 双编码器已经把 PB6~PB9 / PA6~PA7 占满，
  *              硬件 I2C1(PB6/PB7) 与 I2C2(PB10/PB11) 都会和它们冲突；
  *            - 软件 SPI 只占用 4 个任意 GPIO，引脚分配自由，
  *              以后换更大封装的板子也能直接平移，不用重新规划复用功能。
  *
  *          芯片识别：WHO_AM_I (0x75)
  *            0x70 = MPU6500   0x71 = MPU9250   0x73 = MPU6500(部分批次)
  *            0x68 = MPU6050 / MPU6000（同一驱动可直接兼容）
  ******************************************************************************
  */

#ifndef __IMU_H__
#define __IMU_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/* ---- MPU6500 寄存器地址（与 MPU6050 兼容部分） ------------------------- */
#define MPU_REG_SMPLRT_DIV      0x19
#define MPU_REG_CONFIG          0x1A
#define MPU_REG_GYRO_CONFIG     0x1B
#define MPU_REG_ACCEL_CONFIG    0x1C
#define MPU_REG_INT_PIN_CFG     0x37
#define MPU_REG_INT_ENABLE      0x38
#define MPU_REG_INT_STATUS      0x3A
#define MPU_REG_ACCEL_XOUT_H    0x3B
#define MPU_REG_TEMP_OUT_H      0x41
#define MPU_REG_GYRO_XOUT_H     0x43
#define MPU_REG_USER_CTRL       0x6A
#define MPU_REG_PWR_MGMT_1      0x6B
#define MPU_REG_PWR_MGMT_2      0x6C
#define MPU_REG_WHO_AM_I        0x75

/** USER_CTRL 位定义：bit4 = I2C_IF_DIS。
 *  MPU6500/MPU9250 上电默认是 I2C 模式，必须置这一位 SPI 才会响应，
 *  这是软件 SPI 读不到数据最常见的原因。 */
#define MPU_USERCTRL_I2C_IF_DIS 0x10

/** 量程配置 */
#define IMU_GYRO_FS_SEL         3       /* 3 = ±2000 dps */
#define IMU_ACCEL_FS_SEL        1       /* 1 = ±4g */

/** 灵敏度 (LSB per unit)，由量程决定 */
#define IMU_GYRO_LSB_PER_DPS    16.4f   /* ±2000dps */
#define IMU_ACCEL_LSB_PER_G     8192.0f /* ±4g */

/** 数据就绪采样率：DLPF 打开时陀螺输出 1kHz，200Hz = 1000/(1+4) */
#define IMU_SMPLRT_DIV          4

/**
 * @brief  初始化 MPU6500
 * @retval 0 成功；1 读不到器件（WHO_AM_I 不匹配或 SPI 无响应）
 * @note   失败时不会死等，由 App 决定是否降级运行（无 IMU 也能跑循迹）。
 */
uint8_t Imu_Init(void);

/**
 * @brief  读取一次加速度/角速度原始数据（14 字节突发）
 * @note   由控制周期调用。读取前应确认 Imu_DataReady() 为真。
 */
void Imu_ReadData(void);

/** 数据就绪标志：由 EXTI0 中断置位，Imu_ReadData() 读完后清零 */
uint8_t Imu_DataReady(void);

/** 原始数据（int16，单位 LSB） */
int16_t Imu_GetAccelX(void);
int16_t Imu_GetAccelY(void);
int16_t Imu_GetAccelZ(void);
int16_t Imu_GetGyroX(void);
int16_t Imu_GetGyroY(void);
int16_t Imu_GetGyroZ(void);

/** 换算成物理量 */
float Imu_GetAccelG(uint8_t axis);      /* axis: 0=X 1=Y 2=Z，单位 g */
float Imu_GetGyroDps(uint8_t axis);     /* axis: 0=X 1=Y 2=Z，单位 °/s */

/**
 * @brief  陀螺仪零偏静态校准
 * @param  samples  采样点数（约每 5ms 一个点，500 点 ≈ 2.5 秒）
 * @retval 0 成功；1 校准过程中检测到明显运动（建议放稳后重试）
 * @note   上电时车必须静止。零偏不校准会导致 Yaw 持续漂移。
 */
uint8_t Imu_CalibrateGyro(uint16_t samples);

/** 读取校准得到的零偏值 (°/s)，调试用 */
float Imu_GetGyroBias(uint8_t axis);

/** 最近一次成功读取的器件 ID */
uint8_t Imu_GetDeviceID(void);

#ifdef __cplusplus
}
#endif

#endif /* __IMU_H__ */
