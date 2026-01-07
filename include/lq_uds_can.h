/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDS over CAN using ISO-TP Transport
 * 
 * Integrates UDS (ISO 14229) with ISO-TP (ISO 15765-2) for
 * diagnostic services over CAN networks.
 */

#ifndef LQ_UDS_CAN_H_
#define LQ_UDS_CAN_H_

#include "lq_uds.h"
#include "lq_isotp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * UDS over CAN Configuration
 * ============================================================================ */

/**
 * @brief UDS over CAN instance
 * 
 * Combines UDS server with ISO-TP transport layer
 */
struct lq_uds_can {
    struct lq_uds_server uds;          /* UDS server */
    struct lq_isotp_channel isotp;     /* ISO-TP channel */
    struct lq_uds_transport transport;  /* Transport adapter */
    
    /* Buffers */
    uint8_t rx_buf[4095];              /* ISO-TP RX buffer */
    uint8_t pending_response[4095];    /* Pending UDS response */
    size_t pending_response_len;
    
    /* CAN send callback */
    lq_isotp_can_send_fn can_send;
    void *can_ctx;
};

/* ============================================================================
 * UDS over CAN API
 * ============================================================================ */

/**
 * @brief Initialize UDS over CAN
 * 
 * @param uds_can UDS over CAN instance
 * @param uds_config UDS configuration
 * @param isotp_config ISO-TP configuration
 * @param can_send CAN frame send callback
 * @param can_ctx Context for CAN callback
 * @return 0 on success, negative errno on failure
 */
int lq_uds_can_init(struct lq_uds_can *uds_can,
                    const struct lq_uds_config *uds_config,
                    const struct lq_isotp_config *isotp_config,
                    lq_isotp_can_send_fn can_send,
                    void *can_ctx);

/**
 * @brief Process received CAN frame
 * 
 * Call this when a CAN frame is received that might be for this UDS channel.
 * 
 * @param uds_can UDS over CAN instance
 * @param frame Received CAN frame
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_uds_can_recv_frame(struct lq_uds_can *uds_can,
                          const struct lq_isotp_can_frame *frame,
                          uint64_t now);

/**
 * @brief Process periodic tasks
 * 
 * Call this periodically (e.g., every 10ms) for timing and flow control.
 * 
 * @param uds_can UDS over CAN instance
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_uds_can_periodic(struct lq_uds_can *uds_can, uint64_t now);

/**
 * @brief Get current UDS session
 * 
 * @param uds_can UDS over CAN instance
 * @return Current session type
 */
enum lq_uds_session lq_uds_can_get_session(const struct lq_uds_can *uds_can);

/**
 * @brief Get current security level
 * 
 * @param uds_can UDS over CAN instance
 * @return Current security level
 */
enum lq_uds_security_level lq_uds_can_get_security_level(const struct lq_uds_can *uds_can);

#ifdef __cplusplus
}
#endif

#endif /* LQ_UDS_CAN_H_ */
