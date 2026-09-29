/**
  ******************************************************************************
  * @file    led.c
  * @brief   心跳灯与蜂鸣器实现
  ******************************************************************************
  */

#include "led.h"

static LedMode_t s_mode       = LED_MODE_OFF;
static uint32_t  s_elapsed_ms = 0;

/* 蜂鸣器剩余鸣响时间 (ms)，0 = 不响 */
static uint16_t  s_beep_ms    = 0;

/* ==========================================================================
 * 底层
 * ========================================================================== */

static void led_write(uint8_t on)
{
#if LED_ACTIVE_HIGH
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

static void buzzer_write(uint8_t on)
{
#if BUZZER_ACTIVE_HIGH
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

/* ==========================================================================
 * 对外接口
 * ========================================================================== */

void Led_Init(void)
{
    s_mode = LED_MODE_OFF;
    s_elapsed_ms = 0;
    s_beep_ms = 0;
    led_write(0);
    buzzer_write(0);
}

void Led_SetMode(LedMode_t mode)
{
    if (s_mode != mode) {
        s_mode = mode;
        s_elapsed_ms = 0;       /* 换模式时相位归零，避免看起来"卡住" */
    }
}

void Led_Task(uint16_t period_ms)
{
    uint8_t on = 0;

    s_elapsed_ms += period_ms;

    switch (s_mode) {
    case LED_MODE_OFF:
        on = 0;
        break;

    case LED_MODE_BUSY:
        on = 1;
        break;

    case LED_MODE_TRACK:
        /* 1Hz：500ms 亮 500ms 灭 */
        on = (uint8_t)((s_elapsed_ms % 1000U) < 500U);
        break;

    case LED_MODE_RC:
        /* 4Hz：125ms 亮 125ms 灭 */
        on = (uint8_t)((s_elapsed_ms % 250U) < 125U);
        break;

    case LED_MODE_FAULT:
        /* 双闪：200ms 亮 / 200ms 灭 / 200ms 亮 / 600ms 灭 */
        {
            uint32_t t = s_elapsed_ms % 1200U;
            on = (uint8_t)((t < 200U) || (t >= 400U && t < 600U));
        }
        break;

    default:
        on = 0;
        break;
    }

    led_write(on);

    /* ---- 蜂鸣器计时 ---- */
    if (s_beep_ms > 0) {
        if (s_beep_ms <= period_ms) {
            s_beep_ms = 0;
            buzzer_write(0);
        } else {
            s_beep_ms = (uint16_t)(s_beep_ms - period_ms);
        }
    }
}

void Buzzer_On(void)
{
    s_beep_ms = 0;              /* 0 表示"持续响"，不参与倒计时 */
    buzzer_write(1);
}

void Buzzer_Off(void)
{
    s_beep_ms = 0;
    buzzer_write(0);
}

void Buzzer_Beep(uint16_t ms)
{
    s_beep_ms = ms;
    buzzer_write(1);
}
