/*
 * Event Crosschecker Implementation
 * Dual-channel safety system for SIL3/ASIL-D applications
 */

#include "lq_event_crosscheck.h"
#include "lq_platform.h"  /* For lq_uart_send, lq_gpio_set, lq_get_tick_ms */
#include <string.h>

/* CRC-16/CCITT (polynomial 0x1021) */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/* Calculate CRC for crosscheck packet (excludes CRC field itself) */
static uint16_t calculate_packet_crc(const struct lq_crosscheck_packet *pkt)
{
    /* CRC covers: magic_start through value (12 bytes) */
    return crc16_ccitt((const uint8_t *)pkt, 
                       offsetof(struct lq_crosscheck_packet, crc16));
}

/* Trigger fail-safe state */
static void trigger_fail_safe(struct lq_crosscheck_ctx *ctx)
{
    if (ctx->state != LQ_CROSSCHECK_FAIL_SAFE) {
        ctx->state = LQ_CROSSCHECK_FAIL_SAFE;
        
        /* Set fail GPIO high - both MCUs monitor this */
        lq_gpio_set(ctx->fail_gpio, true);
    }
}

/* Add event to pending queue */
static int enqueue_pending(struct lq_crosscheck_ctx *ctx,
                          const struct lq_crosscheck_packet *pkt,
                          uint32_t timestamp_ms)
{
    if (ctx->pending_count >= LQ_CROSSCHECK_QUEUE_SIZE) {
        /* Queue full - this is a critical error */
        trigger_fail_safe(ctx);
        return -1;
    }
    
    struct lq_crosscheck_pending *pending = &ctx->pending[ctx->pending_tail];
    memcpy(&pending->packet, pkt, sizeof(*pkt));
    pending->timestamp_ms = timestamp_ms;
    pending->waiting = true;
    
    ctx->pending_tail = (uint8_t)((ctx->pending_tail + 1) % LQ_CROSSCHECK_QUEUE_SIZE);
    ctx->pending_count++;
    
    return 0;
}

/* Remove oldest pending event */
static void dequeue_pending(struct lq_crosscheck_ctx *ctx)
{
    if (ctx->pending_count > 0) {
        ctx->pending[ctx->pending_head].waiting = false;
        ctx->pending_head = (uint8_t)((ctx->pending_head + 1) % LQ_CROSSCHECK_QUEUE_SIZE);
        ctx->pending_count--;
    }
}

/* Find and verify matching event in pending queue */
static int verify_received_event(struct lq_crosscheck_ctx *ctx,
                                 const struct lq_crosscheck_packet *rx_pkt)
{
    /* Should match oldest pending event (FIFO order) */
    if (ctx->pending_count == 0) {
        /* Received event we didn't send - sequence error */
        ctx->state = LQ_CROSSCHECK_SEQUENCE_ERROR;
        trigger_fail_safe(ctx);
        return -1;
    }
    
    struct lq_crosscheck_pending *pending = &ctx->pending[ctx->pending_head];
    
    /* Verify match */
    if (pending->packet.event_type != rx_pkt->event_type ||
        pending->packet.target_id != rx_pkt->target_id ||
        pending->packet.value != rx_pkt->value) {
        /* Mismatch - MCUs disagree on output */
        ctx->mismatches++;
        ctx->state = LQ_CROSSCHECK_MISMATCH;
        trigger_fail_safe(ctx);
        return -1;
    }
    
    /* Match successful */
    ctx->events_verified++;
    dequeue_pending(ctx);
    ctx->state = LQ_CROSSCHECK_OK;
    
    return 0;
}

/* Receive state machine */
enum rx_state {
    RX_WAIT_START,
    RX_COLLECTING,
    RX_COMPLETE
};

/* Persistent RX state */
static struct {
    enum rx_state state;
    uint8_t buffer[sizeof(struct lq_crosscheck_packet)];
    uint8_t index;
} g_rx_state = {RX_WAIT_START, {0}, 0};

int lq_crosscheck_init(struct lq_crosscheck_ctx *ctx,
                       uint8_t uart_id,
                       uint16_t timeout_ms,
                       uint8_t fail_gpio)
{
    if (!ctx) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->uart_id = uart_id;
    ctx->timeout_ms = timeout_ms;
    ctx->fail_gpio = fail_gpio;
    ctx->enabled = true;
    ctx->state = LQ_CROSSCHECK_OK;
    ctx->tx_sequence = 0;
    ctx->rx_sequence = 0;
    
    /* Initialize fail GPIO as output, set low initially */
    lq_gpio_set(fail_gpio, false);
    
    return 0;
}

int lq_crosscheck_send_event(struct lq_crosscheck_ctx *ctx,
                              const struct lq_output_event *event)
{
    if (!ctx || !event || !ctx->enabled) {
        return -1;
    }
    
    /* Already in fail-safe - don't send */
    if (ctx->state == LQ_CROSSCHECK_FAIL_SAFE) {
        return -1;
    }
    
    /* Build packet */
    struct lq_crosscheck_packet pkt = {
        .magic_start = LQ_CROSSCHECK_MAGIC_START,
        .sequence = ctx->tx_sequence,
        .event_type = event->type,
        .flags = (uint8_t)event->flags,
        .target_id = event->target_id,
        .value = event->value,
        .magic_end = LQ_CROSSCHECK_MAGIC_END,
        .padding = 0
    };
    
    /* Calculate CRC */
    pkt.crc16 = calculate_packet_crc(&pkt);
    
    /* Send over UART */
    int ret = lq_uart_send(ctx->uart_id, (const uint8_t *)&pkt, sizeof(pkt));
    if (ret != 0) {
        return ret;
    }
    
    /* Add to pending queue for verification */
    uint32_t now_ms = lq_get_tick_ms();
    ret = enqueue_pending(ctx, &pkt, now_ms);
    if (ret != 0) {
        return ret;
    }
    
    /* Update statistics and sequence */
    ctx->events_sent++;
    ctx->tx_sequence++;
    
    return 0;
}

int lq_crosscheck_process_byte(struct lq_crosscheck_ctx *ctx, uint8_t byte)
{
    if (!ctx || !ctx->enabled) {
        return -1;
    }
    
    switch (g_rx_state.state) {
        case RX_WAIT_START:
            if (byte == LQ_CROSSCHECK_MAGIC_START) {
                g_rx_state.buffer[0] = byte;
                g_rx_state.index = 1;
                g_rx_state.state = RX_COLLECTING;
            }
            break;
            
        case RX_COLLECTING:
            g_rx_state.buffer[g_rx_state.index++] = byte;
            
            if (g_rx_state.index >= sizeof(struct lq_crosscheck_packet)) {
                g_rx_state.state = RX_COMPLETE;
            }
            break;
            
        case RX_COMPLETE:
            /* Packet complete - process it */
            {
                struct lq_crosscheck_packet *rx_pkt = 
                    (struct lq_crosscheck_packet *)g_rx_state.buffer;
                
                /* Verify magic bytes */
                if (rx_pkt->magic_end != LQ_CROSSCHECK_MAGIC_END) {
                    /* Framing error */
                    g_rx_state.state = RX_WAIT_START;
                    g_rx_state.index = 0;
                    break;
                }
                
                /* Verify CRC */
                uint16_t calc_crc = calculate_packet_crc(rx_pkt);
                if (calc_crc != rx_pkt->crc16) {
                    ctx->crc_errors++;
                    ctx->state = LQ_CROSSCHECK_CRC_ERROR;
                    trigger_fail_safe(ctx);
                    g_rx_state.state = RX_WAIT_START;
                    g_rx_state.index = 0;
                    return -1;
                }
                
                /* Verify sequence number */
                if (rx_pkt->sequence != ctx->rx_sequence) {
                    ctx->state = LQ_CROSSCHECK_SEQUENCE_ERROR;
                    trigger_fail_safe(ctx);
                    g_rx_state.state = RX_WAIT_START;
                    g_rx_state.index = 0;
                    return -1;
                }
                
                /* Verify event matches pending */
                verify_received_event(ctx, rx_pkt);
                
                /* Update expected sequence */
                ctx->rx_sequence++;
                
                /* Reset for next packet */
                g_rx_state.state = RX_WAIT_START;
                g_rx_state.index = 0;
            }
            break;
    }
    
    return 0;
}

int lq_crosscheck_check_timeouts(struct lq_crosscheck_ctx *ctx,
                                  uint32_t current_ms)
{
    if (!ctx || !ctx->enabled) {
        return 0;
    }
    
    /* Already in fail-safe */
    if (ctx->state == LQ_CROSSCHECK_FAIL_SAFE) {
        return -1;
    }
    
    /* Check oldest pending event for timeout */
    if (ctx->pending_count > 0) {
        struct lq_crosscheck_pending *oldest = &ctx->pending[ctx->pending_head];
        
        if (oldest->waiting) {
            uint32_t elapsed_ms = current_ms - oldest->timestamp_ms;
            
            if (elapsed_ms > ctx->timeout_ms) {
                /* Timeout - other MCU failed to respond */
                ctx->timeouts++;
                ctx->state = LQ_CROSSCHECK_TIMEOUT;
                trigger_fail_safe(ctx);
                return -1;
            }
        }
    }
    
    return 0;
}

enum lq_crosscheck_state lq_crosscheck_get_state(const struct lq_crosscheck_ctx *ctx)
{
    return ctx ? ctx->state : LQ_CROSSCHECK_FAIL_SAFE;
}

bool lq_crosscheck_is_failed(const struct lq_crosscheck_ctx *ctx)
{
    return ctx && ctx->state == LQ_CROSSCHECK_FAIL_SAFE;
}

void lq_crosscheck_reset(struct lq_crosscheck_ctx *ctx)
{
    if (!ctx) {
        return;
    }
    
    /* WARNING: This is for testing only - not safe in production */
    ctx->state = LQ_CROSSCHECK_OK;
    ctx->pending_head = 0;
    ctx->pending_tail = 0;
    ctx->pending_count = 0;
    
    /* Clear fail GPIO */
    lq_gpio_set(ctx->fail_gpio, false);
}

void lq_crosscheck_get_stats(const struct lq_crosscheck_ctx *ctx,
                             uint32_t *sent,
                             uint32_t *verified,
                             uint32_t *timeouts,
                             uint32_t *mismatches)
{
    if (!ctx) {
        return;
    }
    
    if (sent) *sent = ctx->events_sent;
    if (verified) *verified = ctx->events_verified;
    if (timeouts) *timeouts = ctx->timeouts;
    if (mismatches) *mismatches = ctx->mismatches;
}
