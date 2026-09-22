/**
  ******************************************************************************
  * @file    app.h
  * @brief   应用层：模式状态机 + 裸机时间片调度
  *
  *          调度结构（裸机，不用 FreeRTOS）：
  *
  *            SysTick (1ms 中断)
  *              └─> App_Tick_1ms()  累加毫秒数
  *                     └─> 每 CTRL_PERIOD_MS 置一次 s_ctrl_flag
  *
  *            main() while(1)
  *              ├─> Comm_Update()          ← 每圈都跑，保证串口不丢数据
  *              └─> if (控制标志) 控制任务   ← 严格 5ms 周期
  *                     ├─ Imu_ReadData + Filter_Update
  *                     ├─ Track_ReadSensors
  *                     ├─ Encoder_Update
  *                     ├─ 按模式调度 Control_*
  *                     └─ Led_Task
  *
  *          为什么不用 FreeRTOS：
  *            F103C8T6 只有 20KB RAM，FreeRTOS 的任务栈 + 内核对象
  *            会吃掉 3~4KB，而本项目的任务只有"控制"和"通信"两个，
  *            且控制周期固定，时间片轮询完全够用，调试也更直观
  *            （不用追任务切换和优先级反转问题）。
  *            换到 F407VET6（192KB RAM）后如果确实需要多任务再迁移。
  ******************************************************************************
  */

#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_config.h"

/** 运行模式 */
typedef enum {
    APP_MODE_STOP  = 0,     /* 停机（默认，上电后不动作） */
    APP_MODE_TRACK = 1,     /* 循迹 */
    APP_MODE_RC    = 2,     /* 蓝牙遥控 */
    APP_MODE_DEBUG = 3      /* 调试：只回传数据，电机不输出 */
} AppMode_t;

/** 遥控模式下的最大目标速度（counts/窗口），对应上位机给的 ±100% */
#define APP_RC_MAX_TARGET       60

/**
 * @brief  应用层初始化
 * @note   必须在 Bsp_Init() 之后调用。
 *         内部会做 IMU 初始化与零偏校准（约 2~3 秒），期间心跳灯常亮。
 */
void App_Init(void);

/**
 * @brief  SysTick 1ms 中断回调
 * @note   必须在 SysTick_Handler 里调用，且要同时调用 HAL_IncTick()。
 */
void App_Tick_1ms(void);

/**
 * @brief  主循环任务
 * @note   在 while(1) 里无条件反复调用。
 */
void App_Loop(void);

/* ---- 供通信层调用的控制接口 ---- */

/** 切换模式，返回 0 成功、1 参数非法 */
uint8_t App_SetMode(AppMode_t mode);

/** 当前模式 */
AppMode_t App_GetMode(void);

/** 设置遥控速度（-100 ~ +100，百分比） */
void App_SetRemoteSpeed(int8_t left_pct, int8_t right_pct);

/** 请求陀螺仪零偏校准（下一个控制周期执行） */
void App_RequestImuCalib(void);

/** 开启/关闭航向锁定 */
void App_SetYawHold(uint8_t enable, float target_yaw);

/** 遥测回传开关 */
void App_SetTelemetryEnabled(uint8_t enable);
uint8_t App_IsTelemetryEnabled(void);

/** IMU 是否可用（初始化成功且校准完成） */
uint8_t App_IsImuReady(void);

/** 系统运行毫秒数 */
uint32_t App_GetTickMs(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
