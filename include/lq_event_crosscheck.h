/*
 * Event Crosschecker for Dual-Channel Safety Systems (SIL3/ASIL-D)
 * 
 * Provides event verification between two independent MCUs for
 * safety-critical applications requiring redundant processing.
 * 
 * Architecture:
 *   MCU1 <--UART--> MCU2
 *   Each MCU:
 *   1. Generates events independently
 *   2. Sends event ID + value over UART
 *   3. Waits to receive matching event from other MCU
 *   4. If mismatch or timeout: trigger fail-safe GPIO
 *   5. Both MCUs monitor fail GPIO as input → enter safe state
 * 
 * Safety Features:
 *   - Timeout detection (max time to receive matching event)
 *   - Event sequence verification
 *   - Mismatch detection (ID or value differ)
 *   - Fail-safe output (both MCUs enter safe state)
 *   - CRC protection on serial protocol
 * 
 * Usage:
 *   1. Configure in DTS with lq,event-crosscheck node
 *   2. Call lq_crosscheck_init() at startup
 *   3. Hook into lq_generated_dispatch_outputs()
 *   4. Monitor fail GPIO as input for safe state trigger
 */

#ifndef LQ_EVENT_CROSSCHECK_H_
#define LQ_EVENT_CROSSCHECK_H_

#include <stdint.h>
#include <stdbool.h>
#include "lq_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum events in verification queue */
#ifndef LQ_CROSSCHECK_QUEUE_SIZE
#define LQ_CROSSCHECK_QUEUE_SIZE 16
#endif

/* Serial protocol magic bytes */
#define LQ_CROSSCHECK_MAGIC_START 0xA5
#define LQ_CROSSCHECK_MAGIC_END   0x5A

/**
 * Event crosscheck packet format (sent over UART)
 * Total: 16 bytes (fixed size for deterministic timing)
 */
struct lq_crosscheck_packet {
    uint8_t  magic_start;    /* 0xA5 */
    uint8_t  sequence;       /* Sequence number (0-255, wraps) */
    uint8_t  event_type;     /* LQ_OUTPUT_* type */
    uint8_t  flags;          /* Reserved */
    uint32_t target_id;      /* Output target (GPIO pin, CAN ID, etc) */
    int32_t  value;          /* Output value */
    uint16_t crc16;          /* CRC-16/CCITT of all fields */
    uint8_t  magic_end;      /* 0x5A */
    uint8_t  padding;        /* Align to 16 bytes */
} __attribute__((packed));

/**
 * Crosscheck verification state
 */
enum lq_crosscheck_state {
    LQ_CROSSCHECK_OK = 0,        /* Normal operation */
    LQ_CROSSCHECK_TIMEOUT,       /* Timeout waiting for match */
    LQ_CROSSCHECK_MISMATCH,      /* Event mismatch detected */
    LQ_CROSSCHECK_CRC_ERROR,     /* CRC validation failed */
    LQ_CROSSCHECK_SEQUENCE_ERROR,/* Sequence number out of order */
    LQ_CROSSCHECK_FAIL_SAFE      /* Permanent fail-safe state */
};

/**
 * Pending event awaiting verification
 */
struct lq_crosscheck_pending {
    struct lq_crosscheck_packet packet;
    uint32_t timestamp_ms;       /* When event was sent */
    bool     waiting;            /* Waiting for match from other MCU */
};

/**
 * Event crosscheck context
 */
struct lq_crosscheck_ctx {
    /* Configuration */
    uint8_t  uart_id;            /* UART peripheral for crosscheck */
    uint16_t timeout_ms;         /* Max time to wait for matching event */
    uint8_t  fail_gpio;          /* GPIO to set high on failure */
    bool     enabled;            /* Crosscheck enabled */
    
    /* Runtime state */
    uint8_t  tx_sequence;        /* Next TX sequence number */
    uint8_t  rx_sequence;        /* Expected RX sequence number */
    enum lq_crosscheck_state state;
    
    /* Pending verification queue */
    struct lq_crosscheck_pending pending[LQ_CROSSCHECK_QUEUE_SIZE];
    uint8_t  pending_head;
    uint8_t  pending_tail;
    uint8_t  pending_count;
    
    /* Statistics */
    uint32_t events_sent;
    uint32_t events_verified;
    uint32_t timeouts;
    uint32_t mismatches;
    uint32_t crc_errors;
};

/**
 * Initialize event crosschecker
 * 
 * @param ctx        Crosscheck context
 * @param uart_id    UART peripheral ID for crosscheck communication
 * @param timeout_ms Maximum time to wait for matching event (typical: 10-100ms)
 * @param fail_gpio  GPIO pin to set high on failure (both MCUs monitor this)
 * @return 0 on success, negative on error
 */
int lq_crosscheck_init(struct lq_crosscheck_ctx *ctx,
                       uint8_t uart_id,
                       uint16_t timeout_ms,
                       uint8_t fail_gpio);

/**
 * Send event to other MCU for verification
 * Called after local event generation
 * 
 * @param ctx   Crosscheck context
 * @param event Output event to verify
 * @return 0 on success, negative on error
 */
int lq_crosscheck_send_event(struct lq_crosscheck_ctx *ctx,
                              const struct lq_output_event *event);

/**
 * Process received bytes from other MCU
 * Call this from UART RX ISR or polling loop
 * 
 * @param ctx  Crosscheck context
 * @param byte Received byte from UART
 * @return 0 on success, negative on error
 */
int lq_crosscheck_process_byte(struct lq_crosscheck_ctx *ctx, uint8_t byte);

/**
 * Check for timeout events (call periodically, e.g., 10ms)
 * If any pending event exceeds timeout, trigger fail-safe
 * 
 * @param ctx         Crosscheck context
 * @param current_ms  Current system time in milliseconds
 * @return 0 if OK, negative if timeout detected
 */
int lq_crosscheck_check_timeouts(struct lq_crosscheck_ctx *ctx,
                                  uint32_t current_ms);

/**
 * Get current crosscheck state
 * 
 * @param ctx Crosscheck context
 * @return Current state
 */
enum lq_crosscheck_state lq_crosscheck_get_state(const struct lq_crosscheck_ctx *ctx);

/**
 * Check if in fail-safe state
 * 
 * @param ctx Crosscheck context
 * @return true if fail-safe triggered
 */
bool lq_crosscheck_is_failed(const struct lq_crosscheck_ctx *ctx);

/**
 * Reset crosscheck state (for testing only - not safe in production)
 * 
 * @param ctx Crosscheck context
 */
void lq_crosscheck_reset(struct lq_crosscheck_ctx *ctx);

/**
 * Get statistics
 * 
 * @param ctx         Crosscheck context
 * @param sent        [out] Number of events sent
 * @param verified    [out] Number of events verified
 * @param timeouts    [out] Number of timeout failures
 * @param mismatches  [out] Number of mismatch failures
 */
void lq_crosscheck_get_stats(const struct lq_crosscheck_ctx *ctx,
                             uint32_t *sent,
                             uint32_t *verified,
                             uint32_t *timeouts,
                             uint32_t *mismatches);

#ifdef __cplusplus
}
#endif

#endif /* LQ_EVENT_CROSSCHECK_H_ */
