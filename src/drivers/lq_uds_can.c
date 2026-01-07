/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDS over CAN Implementation
 */

#include "lq_uds_can.h"
#include "lq_platform.h"
#include <string.h>
#include <errno.h>

/* ============================================================================
 * Transport Layer Adapter
 * ============================================================================ */

static int uds_can_transport_send(void *ctx, const uint8_t *data, size_t len)
{
    struct lq_uds_can *uds_can = (struct lq_uds_can *)ctx;
    
    /* Store response to send via ISO-TP */
    if (len > sizeof(uds_can->pending_response)) {
        return -EMSGSIZE;
    }
    
    memcpy(uds_can->pending_response, data, len);
    uds_can->pending_response_len = len;
    
    return 0;
}

static size_t uds_can_transport_get_max_payload(void *ctx)
{
    (void)ctx;
    return 4095;  /* ISO-TP max */
}

static const struct lq_uds_transport_ops uds_can_transport_ops = {
    .send = uds_can_transport_send,
    .get_max_payload = uds_can_transport_get_max_payload
};

/* ============================================================================
 * Public API
 * ============================================================================ */

int lq_uds_can_init(struct lq_uds_can *uds_can,
                    const struct lq_uds_config *uds_config,
                    const struct lq_isotp_config *isotp_config,
                    lq_isotp_can_send_fn can_send,
                    void *can_ctx)
{
    if (!uds_can || !uds_config || !isotp_config || !can_send) {
        return -EINVAL;
    }
    
    memset(uds_can, 0, sizeof(*uds_can));
    
    /* Store CAN callback */
    uds_can->can_send = can_send;
    uds_can->can_ctx = can_ctx;
    
    /* Initialize ISO-TP */
    int ret = lq_isotp_init(&uds_can->isotp, isotp_config, uds_can->rx_buf);
    if (ret < 0) {
        return ret;
    }
    
    /* Setup UDS transport adapter */
    uds_can->transport.ops = &uds_can_transport_ops;
    uds_can->transport.ctx = uds_can;
    
    /* Initialize UDS server with adapted config */
    struct lq_uds_config uds_cfg = *uds_config;
    uds_cfg.transport = &uds_can->transport;
    
    ret = lq_uds_server_init(&uds_can->uds, &uds_cfg);
    if (ret < 0) {
        return ret;
    }
    
    LQ_LOG_INF("UDS-CAN", "Initialized (RX=0x%03X TX=0x%03X)",
               isotp_config->rx_id, isotp_config->tx_id);
    
    return 0;
}

int lq_uds_can_recv_frame(struct lq_uds_can *uds_can,
                          const struct lq_isotp_can_frame *frame,
                          uint64_t now)
{
    if (!uds_can || !frame) {
        return -EINVAL;
    }
    
    /* Pass to ISO-TP */
    int ret = lq_isotp_recv(&uds_can->isotp, frame,
                            uds_can->can_send, uds_can->can_ctx, now);
    if (ret < 0) {
        return ret;
    }
    
    /* Check if complete UDS request received */
    const uint8_t *request_data;
    size_t request_len;
    
    if (lq_isotp_rx_available(&uds_can->isotp, &request_data, &request_len)) {
        /* Process UDS request */
        LQ_LOG_DBG("UDS-CAN", "Processing UDS request: SID=0x%02X len=%zu",
                   request_data[0], request_len);
        
        ret = lq_uds_server_process_request(&uds_can->uds,
                                            request_data, request_len, now);
        
        /* Acknowledge ISO-TP reception */
        lq_isotp_rx_ack(&uds_can->isotp);
        
        /* Send response if available */
        if (uds_can->pending_response_len > 0) {
            ret = lq_isotp_send(&uds_can->isotp,
                               uds_can->pending_response,
                               uds_can->pending_response_len,
                               uds_can->can_send, uds_can->can_ctx, now);
            
            uds_can->pending_response_len = 0;
            
            if (ret < 0) {
                LQ_LOG_ERR("UDS-CAN", "Failed to send response: %d", ret);
                return ret;
            }
        }
    }
    
    return 0;
}

int lq_uds_can_periodic(struct lq_uds_can *uds_can, uint64_t now)
{
    if (!uds_can) {
        return -EINVAL;
    }
    
    /* ISO-TP periodic (flow control, timeouts) */
    int ret = lq_isotp_periodic(&uds_can->isotp,
                                uds_can->can_send, uds_can->can_ctx, now);
    if (ret < 0 && ret != -ETIMEDOUT) {
        return ret;
    }
    
    /* UDS periodic (session timeout) */
    lq_uds_server_periodic(&uds_can->uds, now);
    
    return 0;
}

enum lq_uds_session lq_uds_can_get_session(const struct lq_uds_can *uds_can)
{
    return uds_can ? lq_uds_get_session(&uds_can->uds) : UDS_SESSION_DEFAULT;
}

enum lq_uds_security_level lq_uds_can_get_security_level(const struct lq_uds_can *uds_can)
{
    return uds_can ? lq_uds_get_security_level(&uds_can->uds) : UDS_SECURITY_LOCKED;
}
