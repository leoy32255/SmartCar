/**
  ******************************************************************************
  * @file    filter.c
  * @brief   互补滤波姿态解算实现
  ******************************************************************************
  */

#include "filter.h"
#include "imu.h"
#include <math.h>

#ifndef M_PI
#define M_PI        3.14159265358979f
#endif
#define RAD_TO_DEG      (180.0f / M_PI)
#define DEG_TO_RAD      (M_PI / 180.0f)

/* 积分步长：由控制周期推导，固定值保证滤波行为可复现 */
#define FILTER_DT       ((float)CTRL_PERIOD_MS / 1000.0f)

static float s_alpha      = FILTER_ALPHA_DEFAULT;
static float s_roll       = 0.0f;   /* 度 */
static float s_pitch      = 0.0f;   /* 度 */
static float s_yaw        = 0.0f;   /* 度，(-180, 180] */
static float s_yaw_total  = 0.0f;   /* 度，不回绕 */
static uint8_t s_first    = 1;

/* ========================================================================== */

void Filter_Init(void)
{
    s_roll = s_pitch = s_yaw = s_yaw_total = 0.0f;
    s_first = 1;
    s_alpha = FILTER_ALPHA_DEFAULT;
}

void Filter_SetAlpha(float alpha)
{
    if (alpha < 0.50f)  alpha = 0.50f;
    if (alpha > 0.999f) alpha = 0.999f;
    s_alpha = alpha;
}

void Filter_Update(void)
{
    float ax = Imu_GetAccelG(0);
    float ay = Imu_GetAccelG(1);
    float az = Imu_GetAccelG(2);

    float gx = Imu_GetGyroDps(0) * DEG_TO_RAD;      /* rad/s */
    float gy = Imu_GetGyroDps(1) * DEG_TO_RAD;
    float gz = Imu_GetGyroDps(2) * DEG_TO_RAD;

    /* ---- 1. 由重力向量解算加计观测角 ---- */
    float acc_roll;
    float acc_pitch;

    {
        /* 用 atan2 而不是 asin，全姿态范围内不出现除零和象限歧义 */
        float norm = sqrtf(ax * ax + ay * ay + az * az);
        if (norm < 0.01f) {
            /* 自由落体或数据异常：本次不用加计修正，只靠陀螺积分 */
            acc_roll  = s_roll;
            acc_pitch = s_pitch;
        } else {
            ax /= norm;
            ay /= norm;
            az /= norm;
            acc_roll  = atan2f(ay, az) * RAD_TO_DEG;
            acc_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * RAD_TO_DEG;
        }
    }

    /* ---- 2. 互补滤波 ---- */
    if (s_first) {
        /* 首次直接用加计观测值起步，避免从 0° 慢慢收敛 */
        s_roll  = acc_roll;
        s_pitch = acc_pitch;
        s_first = 0;
    } else {
        s_roll  = s_alpha * (s_roll  + gx * RAD_TO_DEG * FILTER_DT)
                + (1.0f - s_alpha) * acc_roll;
        s_pitch = s_alpha * (s_pitch + gy * RAD_TO_DEG * FILTER_DT)
                + (1.0f - s_alpha) * acc_pitch;
    }

    /* ---- 3. Yaw：无磁力计，只能纯积分 ----
     * 车体坐标系 Z 轴朝上时，绕 Z 的角速度就是航向变化率。
     * 若模块安装方向不同（例如贴装在竖直面上），需要在这里换轴，
     * 例如改用 -gy 或 gx —— 上电后手动转车 90° 看哪个轴变化最合理即可确定。 */
    {
        float dyaw = gz * RAD_TO_DEG * FILTER_DT;
        s_yaw_total += dyaw;
        s_yaw += dyaw;

        /* 回绕到 (-180, 180]，方便上位机显示与 PID 取误差 */
        while (s_yaw >  180.0f) s_yaw -= 360.0f;
        while (s_yaw <= -180.0f) s_yaw += 360.0f;
    }
}

float Filter_GetYaw(void)       { return s_yaw; }
float Filter_GetPitch(void)     { return s_pitch; }
float Filter_GetRoll(void)      { return s_roll; }
float Filter_GetYawTotal(void)  { return s_yaw_total; }

void Filter_ResetYaw(void)
{
    s_yaw = 0.0f;
    s_yaw_total = 0.0f;
}
