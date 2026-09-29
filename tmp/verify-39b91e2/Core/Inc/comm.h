/**
  ******************************************************************************
  * @file    comm.h
  * @brief   蓝牙通信层（USART2 <-> JDY-31）：帧协议解析与遥测回传
  *
  *          帧格式：
  *            ┌──────┬──────┬──────┬─────┬─────────────┬──────────┐
  *            │ 0xAA │ 0x55 │ CMD  │ LEN │  PAYLOAD    │ CHECKSUM │
  *            └──────┴──────┴──────┴─────┴─────────────┴──────────┘
  *             帧头(2)        功能字  长度   LEN 字节数据   累加和(1)
  *
  *            CHECKSUM = (CMD + LEN + ΣPAYLOAD) & 0xFF
  *            帧头用双字节 0xAA 0x55 而不是单字节，是为了在数据里
  *            恰好出现 0xAA 时不会误判成帧头（接收状态机靠帧头重新同步）。
  *
  *          为什么收发都用中断 + 环形缓冲：
  *            JDY-31 出厂波特率 9600，一帧遥测 17 字节需要约 17.7ms。
  *            如果用 HAL_UART_Transmit 阻塞发送，会直接吃掉 3 个以上控制周期
  *            （5ms），循迹瞬间失控。所以发送必须非阻塞。
  ******************************************************************************
  */

#ifndef __COMM_H__
#define __COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/* ---- 帧定义 ---- */
#define COMM_HEAD1              0xAAU
#define COMM_HEAD2              0x55U
#define COMM_MAX_PAYLOAD        16U

/* ---- 上位机 -> 小车 ---- */
#define CMD_SET_MODE            0x01U   /* payload: [0] = AppMode_t */
#define CMD_RC_DRIVE            0x02U   /* payload: [0] = int8 左轮% , [1] = int8 右轮% */
#define CMD_SET_PARAM           0x03U   /* payload: [0] = param_id, [1..2] = int16 LE */
#define CMD_CALIB_IMU           0x04U   /* 无 payload：触发陀螺仪零偏校准 */
#define CMD_SET_YAW_HOLD        0x05U   /* payload: [0] = enable, [1..2] = int16 目标角×10 */

/* ---- 小车 -> 上位机 ---- */
#define CMD_TELEMETRY           0x81U   /* 13 字节状态回传 */
#define CMD_ACK                 0x82U   /* payload: [0] = 被应答的 CMD, [1] = 状态 */
#define CMD_EVENT               0x83U   /* payload: [0] = 事件码 */

/* ---- 事件码 ---- */
#define EVT_IMU_FAIL            0x01U   /* MPU6500 初始化失败 */
#define EVT_IMU_CALIB_OK        0x02U   /* 零偏校准成功 */
#define EVT_IMU_CALIB_FAIL      0x03U   /* 校准期间检测到运动 */
#define EVT_TRACK_LOST          0x04U   /* 连续丢线，已停车 */

/** 遥测回传周期 (ms)。9600 波特率下一帧 17 字节约 17.7ms，
 *  100ms 周期占空比约 18%，不会挤占接收通道。 */
#define COMM_TELEM_PERIOD_MS    100U

/**
 * @brief  通信层初始化（启动接收中断，清空缓冲）
 */
void Comm_Init(void);

/**
 * @brief  通信层轮询任务：解析收到的帧 + 按周期发遥测
 * @note   在主循环里尽可能频繁地调用（不要只在 5ms 节拍里调），
 *         否则接收缓冲会在高速数据下溢出。
 */
void Comm_Update(void);

/** 发送一帧遥测状态 */
void Comm_SendTelemetry(void);

/** 发送应答帧 */
void Comm_SendAck(uint8_t ack_cmd, uint8_t status);

/** 发送事件帧 */
void Comm_SendEvent(uint8_t event);

/** 是否收到过非法帧（校验错/超长），供调试查看 */
uint16_t Comm_GetErrorCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMM_H__ */
