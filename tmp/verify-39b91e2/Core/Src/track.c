/**
  ******************************************************************************
  * @file    track.c
  * @brief   5 路数字循迹实现：中值滤波 + 加权重心法
  ******************************************************************************
  */

#include "track.h"

/* 传感器引脚表：索引 0 = OUT1(最左) ... 4 = OUT5(最右) */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} track_pin_t;

static const track_pin_t s_pins[TRACK_CH_NUM] = {
    { TRACK1_PORT, TRACK1_PIN },
    { TRACK2_PORT, TRACK2_PIN },
    { TRACK3_PORT, TRACK3_PIN },
    { TRACK4_PORT, TRACK4_PIN },
    { TRACK5_PORT, TRACK5_PIN },
};

static const int16_t s_weight[TRACK_CH_NUM] = {
    TRACK_WEIGHT_1, TRACK_WEIGHT_2, TRACK_WEIGHT_3,
    TRACK_WEIGHT_4, TRACK_WEIGHT_5,
};

/* 每路 3 位移位寄存器，用于 2/3 表决去抖 */
static uint8_t s_hist[TRACK_CH_NUM] = {0};

static uint8_t        s_mask         = 0;
static uint8_t        s_count        = 0;
static int16_t        s_deviation    = 0;
static int16_t        s_last_valid_dev = 0;
static Track_Status_t s_status       = TRACK_STATUS_LOST;
static uint16_t       s_abnormal_cnt = 0;

/** 异常状态持续多少个控制周期后判定为 SUSPECT（5ms × 200 = 1 秒） */
#define TRACK_ABNORMAL_TICKS    200U

/* ========================================================================== */

void Track_Init(void)
{
    for (uint8_t i = 0; i < TRACK_CH_NUM; i++) {
        s_hist[i] = 0;
    }
    Track_Reset();
}

void Track_Reset(void)
{
    s_mask = 0;
    s_count = 0;
    s_deviation = 0;
    s_last_valid_dev = 0;
    s_status = TRACK_STATUS_LOST;
    s_abnormal_cnt = 0;
}

void Track_ReadSensors(void)
{
    uint8_t mask  = 0;
    uint8_t count = 0;

    /* ---- 1. 采样 + 2/3 表决去抖 ----
     * 每个通道保留最近 3 次采样，至少 2 次一致才认可。
     * 可以滤掉电机换向火花、地面接缝造成的单次毛刺。 */
    for (uint8_t i = 0; i < TRACK_CH_NUM; i++) {
        uint8_t level = (HAL_GPIO_ReadPin(s_pins[i].port, s_pins[i].pin) == GPIO_PIN_SET)
                        ? 1U : 0U;

        /* 归一化成"是否压到黑线" */
        uint8_t active = (level == TRACK_ACTIVE_LEVEL) ? 1U : 0U;

        s_hist[i] = (uint8_t)(((s_hist[i] << 1) | active) & 0x07U);

        /* 3 位里 1 的个数 >= 2 则判定有效 */
        {
            uint8_t bits = s_hist[i];
            uint8_t ones = (uint8_t)((bits & 1U) + ((bits >> 1) & 1U) + ((bits >> 2) & 1U));
            if (ones >= 2U) {
                mask |= (uint8_t)(1U << i);
                count++;
            }
        }
    }

    s_mask  = mask;
    s_count = count;

    /* ---- 2. 状态判定 ---- */
    if (count == 0U) {
        /* 全部读到白色：线丢了。按最后已知方向继续修，等线回来。 */
        s_status = TRACK_STATUS_LOST;
        s_abnormal_cnt++;

        /* 没有历史有效值时给 0，车直行找线 */
        s_deviation = s_last_valid_dev;

    } else if (count == TRACK_CH_NUM) {
        /* 全部读到"黑"：可能是垂直于赛道的横线（正常，直接冲过去），
         * 也可能是车被抬起 / 冲出赛道路面。
         * 加权重心在这种情况下天然得 0，正好对应"保持直行"，
         * 所以先按直行处理，持续超过 1 秒才认为出界。 */
        s_status = TRACK_STATUS_ALL;
        s_abnormal_cnt++;
        s_deviation = 0;

    } else {
        /* ---- 3. 正常情况：加权重心法 ---- */
        int32_t wsum = 0;
        int32_t asum = 0;

        for (uint8_t i = 0; i < TRACK_CH_NUM; i++) {
            if (mask & (1U << i)) {
                wsum += s_weight[i];
                asum += 1;
            }
        }

        /* asum 等于 count，在 count>0 的分支里必不为零 */
        s_deviation = (int16_t)((wsum * TRACK_DIR_SIGN) / asum);

        s_last_valid_dev = s_deviation;
        s_status = TRACK_STATUS_OK;
        s_abnormal_cnt = 0;
    }

    /* ---- 4. 异常持续过久 -> 建议上层停车 ---- */
    if (s_abnormal_cnt > TRACK_ABNORMAL_TICKS) {
        s_status = TRACK_STATUS_SUSPECT;
    }
}

int16_t Track_GetDeviation(void)     { return s_deviation; }
uint8_t Track_GetMask(void)          { return s_mask; }
uint8_t Track_GetActiveCount(void)   { return s_count; }
Track_Status_t Track_GetStatus(void) { return s_status; }
