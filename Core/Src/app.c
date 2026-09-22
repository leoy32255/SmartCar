/**
  ******************************************************************************
  * @file    app.c
  * @brief   应用层实现：模式状态机 + 时间片调度
  ******************************************************************************
  */

#include "app.h"
#include "motor.h"
#include "encoder.h"
#include "track.h"
#include "imu.h"
#include "filter.h"
#include "control.h"
#include "comm.h"
#include "led.h"

/* ---- 调度 ---- */
static volatile uint32_t s_tick_ms    = 0;
static volatile uint8_t  s_ctrl_flag  = 0;
static uint8_t           s_tick_div   = 0;

/* ---- 状态 ---- */
static AppMode_t s_mode        = APP_MODE_STOP;
static int8_t    s_rc_left     = 0;
static int8_t    s_rc_right    = 0;
static uint8_t   s_imu_ready   = 0;
static uint8_t   s_telem_en    = 1;
static uint8_t   s_calib_req   = 0;

/** 循迹故障标志：丢线过久后置位，刹车等待上位机重新下指令 */
static uint8_t   s_track_fault = 0;

/* ==========================================================================
 * 内部辅助
 * ========================================================================== */

/** 把遥控百分比换算成目标速度 */
static int16_t rc_pct_to_target(int8_t pct)
{
    return (int16_t)(((int32_t)pct * APP_RC_MAX_TARGET) / 100);
}

/** 进入某个模式时的动作 */
static void app_enter_mode(AppMode_t mode)
{
    switch (mode) {
    case APP_MODE_STOP:
        Control_Stop();
        Motor_Standby(0);               /* 关断 TB6612，最低功耗也最安全 */
        Led_SetMode(LED_MODE_OFF);
        break;

    case APP_MODE_TRACK:
        /* 重新出发：清掉故障、复位采样与 PID 状态，
         * 并把当前朝向记为 0° 基准（航向锁定用）。 */
        s_track_fault = 0;
        Track_Reset();
        Encoder_Reset();
        Control_Stop();
        Control_CaptureYaw();
        Motor_Standby(1);               /* TB6612 使能，否则电机不转 */
        Led_SetMode(LED_MODE_TRACK);
        break;

    case APP_MODE_RC:
        Control_SetDirectMode(1);
        s_rc_left = s_rc_right = 0;
        Control_SetDirectTarget(0, 0);
        Motor_Standby(1);               /* TB6612 使能 */
        Led_SetMode(LED_MODE_RC);
        break;

    case APP_MODE_DEBUG:
        Control_Stop();
        Led_SetMode(LED_MODE_BUSY);
        break;

    default:
        break;
    }
}

/** 退出某个模式时的动作 */
static void app_exit_mode(AppMode_t mode)
{
    if (mode == APP_MODE_RC) {
        /* 退出遥控要关掉直控，否则回到循迹时外环不工作 */
        Control_SetDirectMode(0);
    }
}

/* ==========================================================================
 * 控制周期任务（每 CTRL_PERIOD_MS 执行一次）
 * ========================================================================== */

static void app_control_task(void)
{
    /* ---- 1. 姿态：等中断标志，避免读到半更新的数据 ---- */
    if (s_imu_ready) {
        if (Imu_DataReady()) {
            Imu_ReadData();
            Filter_Update();
        }
    }

    /* ---- 2. 循迹采样（内部含去抖，必须每周期调） ---- */
    Track_ReadSensors();

    /* ---- 3. 编码器测速 ---- */
    Encoder_Update();

    /* ---- 4. 按模式调度 ---- */
    switch (s_mode) {

    case APP_MODE_STOP:
        Motor_Coast();
        break;

    case APP_MODE_TRACK:
        if (s_track_fault) {
            /* 已经判定丢线：保持刹车，等上位机重新切模式 */
            Motor_Brake();
        } else if (Track_GetStatus() == TRACK_STATUS_SUSPECT) {
            /* 连续 1 秒没找到线 —— 可能冲出赛道或被抬起。
             * 直接停车并上报，让操作者介入；比闭眼乱冲安全得多。 */
            s_track_fault = 1;
            Control_Brake();
            Led_SetMode(LED_MODE_FAULT);
            Buzzer_Beep(300);
            Comm_SendEvent(EVT_TRACK_LOST);
        } else {
            Led_SetMode(LED_MODE_TRACK);
            Control_Update();
        }
        break;

    case APP_MODE_RC:
        Control_SetDirectTarget(rc_pct_to_target(s_rc_left),
                                rc_pct_to_target(s_rc_right));
        Control_Update();
        break;

    case APP_MODE_DEBUG:
    default:
        Motor_Coast();
        break;
    }

    /* ---- 5. 心跳灯 ---- */
    Led_Task(CTRL_PERIOD_MS);
}

/* ==========================================================================
 * 初始化
 * ========================================================================== */

void App_Init(void)
{
    /* 先把执行机构置于安全状态，再初始化其他模块。
     * 顺序很重要：如果先初始化传感器再关电机，
     * 万一初始化过程中电机引脚浮空，电机可能乱转。 */
    Motor_Init();
    Motor_Standby(0);

    Led_Init();
    Led_SetMode(LED_MODE_BUSY);

    Track_Init();
    Encoder_Init();
    Filter_Init();
    Control_Init();
    Comm_Init();

    /* ---- IMU 初始化 + 零偏校准 ----
     * 失败不阻塞启动：没有 IMU 依然可以跑循迹（循迹只用红外传感器），
     * 只是航向锁定功能不可用。 */
    s_imu_ready = 0;
    if (Imu_Init() == 0) {
        /* 校准期间车必须静止。这里用阻塞方式，因为此时还没进入主循环，
         * 阻塞不会影响控制节拍。500 点 × 5ms ≈ 2.5 秒。 */
        if (Imu_CalibrateGyro(IMU_CALIB_SAMPLES) == 0) {
            s_imu_ready = 1;
            Comm_SendEvent(EVT_IMU_CALIB_OK);
        } else {
            Comm_SendEvent(EVT_IMU_CALIB_FAIL);
            Buzzer_Beep(200);
        }
    } else {
        Comm_SendEvent(EVT_IMU_FAIL);
        Buzzer_Beep(600);
    }

    /* ---- 进入模式 ----
     * 默认 STOP：必须靠上位机指令才启动（最终产品的正确行为）。
     * 若 bsp_config.h 里配了 APP_AUTOSTART_MODE，则跳过等待直接启动，
     * 方便在还没有上位机的时候做台架测试。 */
    s_mode = APP_MODE_STOP;
    s_track_fault = 0;
    s_telem_en = 1;
    Led_SetMode(LED_MODE_OFF);

#if (APP_AUTOSTART_MODE != 0)
    App_SetMode((AppMode_t)APP_AUTOSTART_MODE);
#endif

    /* ---- 上电提示音 ---- */
    Buzzer_Beep(100);
}

/* ==========================================================================
 * 调度
 * ========================================================================== */

void App_Tick_1ms(void)
{
    s_tick_ms++;

    s_tick_div++;
    if (s_tick_div >= CTRL_PERIOD_MS) {
        s_tick_div = 0;
        s_ctrl_flag = 1;
    }
}

void App_Loop(void)
{
    /* 通信每圈都跑：9600 波特率下一字节约 1ms，
     * 主循环两圈（约 1~2μs）就能取走，完全跟得上。 */
    Comm_Update();

    /* 陀螺仪零偏校准请求：放到这里执行，避免在通信中断上下文里跑长任务 */
    if (s_calib_req) {
        s_calib_req = 0;
        if (s_imu_ready) {
            /* 校准是阻塞的（约 2.5 秒），期间控制任务不会执行。
             * 必须先停车并把 TB6612 关断，否则电机会保持上一次的 PWM
             * 一直转 2.5 秒 —— 而且校准要求车静止，转着校出来的零偏是错的。 */
            Control_Stop();
            Motor_Standby(0);

            Led_SetMode(LED_MODE_BUSY);
            if (Imu_CalibrateGyro(IMU_CALIB_SAMPLES) == 0) {
                Comm_SendEvent(EVT_IMU_CALIB_OK);
                Buzzer_Beep(100);
            } else {
                Comm_SendEvent(EVT_IMU_CALIB_FAIL);
                Buzzer_Beep(500);
            }

            /* 通过 app_enter_mode 恢复模式对应的灯与执行机构状态 */
            app_enter_mode(s_mode);
        }
    }

    /* 控制任务：严格按周期执行 */
    if (s_ctrl_flag) {
        s_ctrl_flag = 0;
        app_control_task();
    }
}

/* ==========================================================================
 * 对外接口
 * ========================================================================== */

uint8_t App_SetMode(AppMode_t mode)
{
    if (mode > APP_MODE_DEBUG) {
        return 1;
    }

    if (mode != s_mode) {
        app_exit_mode(s_mode);
        s_mode = mode;
        app_enter_mode(s_mode);
    } else {
        /* 重复设置同一模式 = 重新出发（清故障、复位采样） */
        s_track_fault = 0;
        Track_Reset();
        Encoder_Reset();
        Control_Stop();
        app_enter_mode(s_mode);
    }

    return 0;
}

AppMode_t App_GetMode(void) { return s_mode; }

void App_SetRemoteSpeed(int8_t left_pct, int8_t right_pct)
{
    if (left_pct >  100) left_pct =  100;
    if (left_pct < -100) left_pct = -100;
    if (right_pct >  100) right_pct =  100;
    if (right_pct < -100) right_pct = -100;

    s_rc_left = left_pct;
    s_rc_right = right_pct;
}

void App_RequestImuCalib(void)
{
    s_calib_req = 1;
}

void App_SetYawHold(uint8_t enable, float target_yaw)
{
    if (enable) {
        /* 目标角传 -32768（0x8000）表示"以当前朝向为基准" */
        Control_SetYawHold(1, target_yaw);
    } else {
        Control_SetYawHold(0, 0.0f);
    }
}

void App_SetTelemetryEnabled(uint8_t enable) { s_telem_en = enable ? 1U : 0U; }
uint8_t App_IsTelemetryEnabled(void)         { return s_telem_en; }
uint8_t App_IsImuReady(void)                 { return s_imu_ready; }
uint32_t App_GetTickMs(void)                 { return s_tick_ms; }
