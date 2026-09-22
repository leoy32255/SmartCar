/**
  ******************************************************************************
  * @file    comm.c
  * @brief   蓝牙通信实现：环形缓冲 + 中断收发 + 帧状态机
  ******************************************************************************
  */

#include "comm.h"
#include "app.h"
#include "control.h"
#include "track.h"
#include "encoder.h"
#include "filter.h"
#include "motor.h"
#include "imu.h"

/* ==========================================================================
 * 环形缓冲
 * ========================================================================== */
#define RX_RING_SIZE    64U     /* 必须是 2 的幂，用位与代替取模 */
#define TX_RING_SIZE    256U

static volatile uint8_t  s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0;     /* 中断写入 */
static volatile uint16_t s_rx_tail = 0;     /* 主循环读出 */

static volatile uint8_t  s_tx_ring[TX_RING_SIZE];
static volatile uint16_t s_tx_head = 0;     /* 主循环写入 */
static volatile uint16_t s_tx_tail = 0;     /* 中断读出 */

static uint16_t s_err_count = 0;

/* ==========================================================================
 * 环形缓冲操作
 * ========================================================================== */

static inline uint8_t rx_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_rx_head + 1U) & (RX_RING_SIZE - 1U));
    if (next == s_rx_tail) {
        return 0;               /* 满，丢弃。宁可丢也要保证不覆盖未读数据 */
    }
    s_rx_ring[s_rx_head] = b;
    s_rx_head = next;
    return 1;
}

static inline uint8_t rx_pop(uint8_t *b)
{
    if (s_rx_tail == s_rx_head) {
        return 0;
    }
    *b = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1U) & (RX_RING_SIZE - 1U));
    return 1;
}

static inline uint8_t tx_push(uint8_t b)
{
    uint16_t next = (uint16_t)((s_tx_head + 1U) & (TX_RING_SIZE - 1U));
    if (next == s_tx_tail) {
        return 0;               /* 满：发送侧溢出，说明遥测发得太频 */
    }
    s_tx_ring[s_tx_head] = b;
    s_tx_head = next;
    return 1;
}

static inline uint8_t tx_pop(uint8_t *b)
{
    if (s_tx_tail == s_tx_head) {
        return 0;
    }
    *b = s_tx_ring[s_tx_tail];
    s_tx_tail = (uint16_t)((s_tx_tail + 1U) & (TX_RING_SIZE - 1U));
    return 1;
}

/** 启动发送：把 TXE 中断打开，硬件会自动一个字节一个字节地取走 */
static void tx_kick(void)
{
    USART2->CR1 |= USART_CR1_TXEIE;
}

/* ==========================================================================
 * 串口中断：完全接管 USART2，不走 HAL 的中断处理
 * ========================================================================== */

/**
 * @brief  USART2 中断服务程序
 * @note   本项目自行实现，因此 CubeMX 生成的 stm32f1xx_it.c 里
 *         不能再有 USART2_IRQHandler，否则链接时重复定义。
 *         （配置 CubeMX 时不要勾选 USART2 的 global interrupt，
 *           或者生成后把那个函数删掉。）
 */
void USART2_IRQHandler(void)
{
    uint32_t sr  = USART2->SR;
    uint32_t cr1 = USART2->CR1;

    /* ---- 接收 ---- */
    if (sr & USART_SR_RXNE) {
        uint8_t b = (uint8_t)(USART2->DR & 0x00FFU);
        (void)rx_push(b);
    } else if (sr & USART_SR_ORE) {
        /* 溢出：上溢说明主循环没能及时取走数据。
         * 必须读 DR 才能清掉 ORE，否则会一直触发中断。 */
        (void)USART2->DR;
    }

    /* ---- 发送：DR 空且允许中断时，补一个字节 ---- */
    if ((cr1 & USART_CR1_TXEIE) && (sr & USART_SR_TXE)) {
        uint8_t b;
        if (tx_pop(&b)) {
            USART2->DR = b;
        } else {
            USART2->CR1 &= ~USART_CR1_TXEIE;    /* 发完了，关中断 */
        }
    }
}

/* ==========================================================================
 * 发送
 * ========================================================================== */

static void comm_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t  sum = (uint8_t)(cmd + len);

    if (len > COMM_MAX_PAYLOAD) {
        return;
    }

    tx_push(COMM_HEAD1);
    tx_push(COMM_HEAD2);
    tx_push(cmd);
    tx_push(len);

    for (uint8_t i = 0; i < len; i++) {
        tx_push(payload[i]);
        sum = (uint8_t)(sum + payload[i]);
    }

    tx_push(sum);
    tx_kick();
}

static void put_i16(uint8_t *buf, int16_t v)
{
    buf[0] = (uint8_t)(v & 0xFF);
    buf[1] = (uint8_t)((v >> 8) & 0xFF);
}

static int16_t get_i16(const uint8_t *buf)
{
    return (int16_t)((int16_t)(buf[0] | ((uint16_t)buf[1] << 8)));
}

void Comm_SendTelemetry(void)
{
    uint8_t  p[13];
    int16_t  spd_l, spd_r;

    Control_GetActualSpeed(&spd_l, &spd_r);

    p[0] = Track_GetMask();
    put_i16(&p[1],  Track_GetDeviation());
    put_i16(&p[3],  spd_l);
    put_i16(&p[5],  spd_r);
    put_i16(&p[7],  (int16_t)(Filter_GetYaw()   * 10.0f));
    put_i16(&p[9],  (int16_t)(Filter_GetPitch() * 10.0f));
    put_i16(&p[11], (int16_t)(Filter_GetRoll()  * 10.0f));

    comm_send_frame(CMD_TELEMETRY, p, sizeof(p));
}

void Comm_SendAck(uint8_t ack_cmd, uint8_t status)
{
    uint8_t p[2];
    p[0] = ack_cmd;
    p[1] = status;
    comm_send_frame(CMD_ACK, p, 2);
}

void Comm_SendEvent(uint8_t event)
{
    uint8_t p[1];
    p[0] = event;
    comm_send_frame(CMD_EVENT, p, 1);
}

/* ==========================================================================
 * 接收状态机
 * ========================================================================== */

typedef enum {
    ST_HEAD1 = 0,
    ST_HEAD2,
    ST_CMD,
    ST_LEN,
    ST_PAYLOAD,
    ST_CHECKSUM
} rx_state_t;

static rx_state_t s_state    = ST_HEAD1;
static uint8_t    s_cmd      = 0;
static uint8_t    s_len      = 0;
static uint8_t    s_idx      = 0;
static uint8_t    s_payload[COMM_MAX_PAYLOAD];
static uint8_t    s_sum      = 0;

static void comm_handle_frame(uint8_t cmd, const uint8_t *p, uint8_t len)
{
    switch (cmd) {

    case CMD_SET_MODE:
        if (len >= 1) {
            uint8_t ok = App_SetMode((AppMode_t)p[0]) ? 1U : 0U;
            Comm_SendAck(cmd, ok);
        }
        break;

    case CMD_RC_DRIVE:
        if (len >= 2) {
            App_SetRemoteSpeed((int8_t)p[0], (int8_t)p[1]);
            Comm_SendAck(cmd, 0);
        }
        break;

    case CMD_SET_PARAM:
        if (len >= 3) {
            uint8_t ok = Control_SetParam((Control_ParamId_t)p[0], get_i16(&p[1]));
            Comm_SendAck(cmd, ok);
        }
        break;

    case CMD_CALIB_IMU:
        App_RequestImuCalib();
        Comm_SendAck(cmd, 0);
        break;

    case CMD_SET_YAW_HOLD:
        if (len >= 3) {
            App_SetYawHold(p[0], (float)get_i16(&p[1]) / 10.0f);
            Comm_SendAck(cmd, 0);
        }
        break;

    default:
        /* 未知功能字：应答错误，方便上位机知道发错了 */
        Comm_SendAck(cmd, 1);
        break;
    }
}

static void comm_feed(uint8_t b)
{
    switch (s_state) {

    case ST_HEAD1:
        if (b == COMM_HEAD1) {
            s_state = ST_HEAD2;
        }
        break;

    case ST_HEAD2:
        if (b == COMM_HEAD2) {
            s_state = ST_CMD;
        } else if (b == COMM_HEAD1) {
            /* 连续两个 0xAA：保持等第二个字节，可能是 0xAA 0xAA 0x55 */
            s_state = ST_HEAD2;
        } else {
            s_state = ST_HEAD1;
        }
        break;

    case ST_CMD:
        s_cmd = b;
        s_state = ST_LEN;
        break;

    case ST_LEN:
        s_len = b;
        s_sum = (uint8_t)(s_cmd + s_len);
        s_idx = 0;
        if (s_len > COMM_MAX_PAYLOAD) {
            /* 长度非法，直接丢掉重找帧头 */
            s_err_count++;
            s_state = ST_HEAD1;
        } else if (s_len == 0) {
            s_state = ST_CHECKSUM;
        } else {
            s_state = ST_PAYLOAD;
        }
        break;

    case ST_PAYLOAD:
        s_payload[s_idx++] = b;
        s_sum = (uint8_t)(s_sum + b);
        if (s_idx >= s_len) {
            s_state = ST_CHECKSUM;
        }
        break;

    case ST_CHECKSUM:
        if (b == s_sum) {
            comm_handle_frame(s_cmd, s_payload, s_len);
        } else {
            s_err_count++;
        }
        s_state = ST_HEAD1;
        break;

    default:
        s_state = ST_HEAD1;
        break;
    }
}

/* ==========================================================================
 * 对外接口
 * ========================================================================== */

void Comm_Init(void)
{
    s_rx_head = s_rx_tail = 0;
    s_tx_head = s_tx_tail = 0;
    s_state = ST_HEAD1;
    s_err_count = 0;

    /* 使能接收中断（Bsp_Init 里已经设过 RXNEIE，这里再确保一次） */
    USART2->CR1 |= USART_CR1_RXNEIE;
}

void Comm_Update(void)
{
    uint8_t b;

    /* ---- 收：把环形缓冲里的字节全部喂给状态机 ----
     * 一次循环最多处理 32 字节，避免极端情况下长时间占住主循环
     * （比如波特率被改错导致收到大量垃圾数据）。 */
    {
        uint8_t budget = 32;
        while (budget-- && rx_pop(&b)) {
            comm_feed(b);
        }
    }

    /* ---- 发：按周期回传遥测 ----
     * 必须用 SysTick 维护的毫秒计数来计时，不能按"本函数被调用的次数"
     * 计数：主循环一圈只有 1~2μs，用调用次数会让遥测以约 1000 倍
     * 的频率发出，直接把发送环形缓冲冲爆（表现为上位机收到残缺帧）。 */
    {
        static uint32_t last_telem_ms = 0;
        uint32_t now = App_GetTickMs();

        if ((uint32_t)(now - last_telem_ms) >= COMM_TELEM_PERIOD_MS) {
            last_telem_ms = now;
            if (App_IsTelemetryEnabled()) {
                Comm_SendTelemetry();
            }
        }
    }
}

uint16_t Comm_GetErrorCount(void)
{
    return s_err_count;
}
