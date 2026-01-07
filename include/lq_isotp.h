/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ISO-TP (ISO 15765-2) - CAN Transport Protocol
 * 
 * Implements multi-frame message segmentation/reassembly for CAN.
 * Used primarily for UDS (ISO 14229) diagnostic services over CAN.
 * 
 * Frame Types:
 * - Single Frame (SF): Messages up to 7 bytes (CAN) or 62 bytes (CAN-FD)
 * - First Frame (FF): Start of multi-frame message
 * - Consecutive Frame (CF): Continuation of multi-frame message
 * - Flow Control (FC): Receiver flow control
 * 
 * Addressing:
 * - Normal: 11-bit or 29-bit CAN IDs
 * - Extended: 29-bit with target/source addressing
 */

#ifndef LQ_ISOTP_H_
#define LQ_ISOTP_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * ISO-TP Frame Types
 * ============================================================================ */

#define ISOTP_PCI_SF        0x00  /* Single Frame */
#define ISOTP_PCI_FF        0x10  /* First Frame */
#define ISOTP_PCI_CF        0x20  /* Consecutive Frame */
#define ISOTP_PCI_FC        0x30  /* Flow Control */

/* Flow Control Status */
#define ISOTP_FC_CTS        0x00  /* Continue To Send */
#define ISOTP_FC_WAIT       0x01  /* Wait */
#define ISOTP_FC_OVFLW      0x02  /* Overflow - abort transfer */

/* ============================================================================
 * ISO-TP Configuration
 * ============================================================================ */

/**
 * @brief ISO-TP channel configuration
 */
struct lq_isotp_config {
    /* CAN IDs */
    uint32_t tx_id;              /* Transmit CAN ID */
    uint32_t rx_id;              /* Receive CAN ID */
    bool use_extended_id;        /* 29-bit CAN ID */
    
    /* Timing parameters (milliseconds) */
    uint32_t n_bs;               /* Block size (0 = unlimited) */
    uint32_t n_st_min;           /* Minimum separation time between CF (0-127ms) */
    uint32_t n_as;               /* Sender timing (default: 1000ms) */
    uint32_t n_ar;               /* Receiver timing (default: 1000ms) */
    uint32_t n_bs_timeout;       /* Block size timeout (default: 1000ms) */
    uint32_t n_cr;               /* Consecutive frame timeout (default: 1000ms) */
    
    /* Buffer sizes */
    size_t tx_buf_size;          /* Transmit buffer size (default: 4095) */
    size_t rx_buf_size;          /* Receive buffer size (default: 4095) */
};

/* ============================================================================
 * ISO-TP State Machine
 * ============================================================================ */

enum lq_isotp_tx_state {
    ISOTP_TX_IDLE = 0,
    ISOTP_TX_SENDING_SF,
    ISOTP_TX_SENDING_FF,
    ISOTP_TX_WAIT_FC,
    ISOTP_TX_SENDING_CF,
};

enum lq_isotp_rx_state {
    ISOTP_RX_IDLE = 0,
    ISOTP_RX_RECEIVING_FF,
    ISOTP_RX_RECEIVING_CF,
};

/**
 * @brief ISO-TP channel state
 */
struct lq_isotp_channel {
    struct lq_isotp_config config;
    
    /* TX state */
    enum lq_isotp_tx_state tx_state;
    const uint8_t *tx_data;
    size_t tx_len;
    size_t tx_offset;
    uint8_t tx_sn;               /* Sequence number (0-15) */
    uint64_t tx_deadline;        /* Timeout deadline */
    
    /* RX state */
    enum lq_isotp_rx_state rx_state;
    uint8_t *rx_buf;
    size_t rx_buf_size;
    size_t rx_len;               /* Total message length */
    size_t rx_offset;            /* Current receive offset */
    uint8_t rx_sn;               /* Expected sequence number */
    uint64_t rx_deadline;        /* Timeout deadline */
    
    /* Flow control */
    uint8_t fc_bs;               /* Block size from peer */
    uint8_t fc_st_min;           /* STmin from peer */
    uint8_t block_count;         /* Frames sent in current block */
    uint64_t next_cf_time;       /* Time to send next CF */
};

/* ============================================================================
 * CAN Frame Interface
 * ============================================================================ */

/**
 * @brief CAN frame for ISO-TP
 */
struct lq_isotp_can_frame {
    uint32_t can_id;
    bool is_extended;
    uint8_t dlc;
    uint8_t data[8];
};

/**
 * @brief CAN send callback
 * 
 * @param ctx User context
 * @param frame CAN frame to send
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_isotp_can_send_fn)(void *ctx, const struct lq_isotp_can_frame *frame);

/* ============================================================================
 * ISO-TP API
 * ============================================================================ */

/**
 * @brief Initialize ISO-TP channel
 * 
 * @param channel ISO-TP channel instance
 * @param config Configuration
 * @param rx_buffer Receive buffer (must be at least rx_buf_size)
 * @return 0 on success, negative errno on failure
 */
int lq_isotp_init(struct lq_isotp_channel *channel,
                  const struct lq_isotp_config *config,
                  uint8_t *rx_buffer);

/**
 * @brief Start ISO-TP transmission
 * 
 * This begins transmitting a message. The data must remain valid until
 * transmission is complete (checked via lq_isotp_tx_done()).
 * 
 * @param channel ISO-TP channel
 * @param data Data to send
 * @param len Data length
 * @param can_send Callback to send CAN frames
 * @param can_ctx Context for CAN send callback
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_isotp_send(struct lq_isotp_channel *channel,
                  const uint8_t *data, size_t len,
                  lq_isotp_can_send_fn can_send, void *can_ctx,
                  uint64_t now);

/**
 * @brief Process received CAN frame
 * 
 * Call this when a CAN frame is received on the channel's RX ID.
 * 
 * @param channel ISO-TP channel
 * @param frame Received CAN frame
 * @param can_send Callback to send CAN frames (for Flow Control)
 * @param can_ctx Context for CAN send callback
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_isotp_recv(struct lq_isotp_channel *channel,
                  const struct lq_isotp_can_frame *frame,
                  lq_isotp_can_send_fn can_send, void *can_ctx,
                  uint64_t now);

/**
 * @brief Process periodic tasks (timeouts, flow control)
 * 
 * Call this periodically (e.g., every 10ms) to handle timing.
 * 
 * @param channel ISO-TP channel
 * @param can_send Callback to send CAN frames
 * @param can_ctx Context for CAN send callback
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_isotp_periodic(struct lq_isotp_channel *channel,
                      lq_isotp_can_send_fn can_send, void *can_ctx,
                      uint64_t now);

/**
 * @brief Check if transmission is complete
 * 
 * @param channel ISO-TP channel
 * @return true if TX idle, false if transmitting
 */
bool lq_isotp_tx_done(const struct lq_isotp_channel *channel);

/**
 * @brief Check if a complete message has been received
 * 
 * @param channel ISO-TP channel
 * @param data Output: pointer to received data (in rx_buf)
 * @param len Output: received data length
 * @return true if message available, false otherwise
 */
bool lq_isotp_rx_available(struct lq_isotp_channel *channel,
                           const uint8_t **data, size_t *len);

/**
 * @brief Acknowledge received message
 * 
 * Call this after processing a received message to free the RX buffer.
 * 
 * @param channel ISO-TP channel
 */
void lq_isotp_rx_ack(struct lq_isotp_channel *channel);

/**
 * @brief Abort current transmission or reception
 * 
 * @param channel ISO-TP channel
 */
void lq_isotp_abort(struct lq_isotp_channel *channel);

#ifdef __cplusplus
}
#endif

#endif /* LQ_ISOTP_H_ */
