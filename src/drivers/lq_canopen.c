/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * CANopen Protocol Implementation
 * Implements the unified protocol driver interface for CANopen
 */

#include "lq_canopen.h"
#include <string.h>
#include <stdlib.h>

/* CANopen Protocol Driver Implementation */

static int canopen_init(struct lq_protocol_driver *proto,
                        const struct lq_protocol_config *config)
{
    if (!proto || !proto->ctx || !config) {
        return -1;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    
    /* Store config in protocol driver */
    proto->config = *config;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->node_id = config->node_address;
    ctx->nmt_state = CANOPEN_STATE_PRE_OPERATIONAL;  /* Start in pre-operational */
    ctx->heartbeat_period_ms = 1000;  /* Default 1s heartbeat */
    
    return 0;
}

static size_t canopen_decode(struct lq_protocol_driver *proto,
                             uint64_t now,
                             const struct lq_protocol_msg *msg,
                             struct lq_event *out_events,
                             size_t max_events)
{
    if (!proto || !msg || !out_events || max_events == 0) {
        return 0;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    size_t num_events = 0;
    
    if (!msg->data) {
        return 0;
    }
    
    uint16_t function_code = msg->address & 0x780;
    uint8_t node_id = msg->address & 0x7F;
    (void)function_code;  /* Reserved for future decoding */
    (void)node_id;  /* Reserved for future decoding */
    
    /* Handle NMT messages */
    if (msg->address == CANOPEN_FC_NMT) {
        if (msg->len == 2) {
            uint8_t cmd = msg->data[0];
            uint8_t target = msg->data[1];
            
            /* Process if addressed to us or broadcast (0) */
            if (target == ctx->node_id || target == 0) {
                switch (cmd) {
                    case CANOPEN_NMT_START:
                        ctx->nmt_state = CANOPEN_STATE_OPERATIONAL;
                        break;
                    case CANOPEN_NMT_STOP:
                        ctx->nmt_state = CANOPEN_STATE_STOPPED;
                        break;
                    case CANOPEN_NMT_PRE_OPERATIONAL:
                        ctx->nmt_state = CANOPEN_STATE_PRE_OPERATIONAL;
                        break;
                }
            }
        }
        return 0;
    }
    
    /* Handle SYNC */
    if (msg->address == CANOPEN_FC_SYNC) {
        ctx->sync_counter++;
        return 0;
    }
    
    /* Handle LSS messages (slave receives from master) */
    if (msg->address == CANOPEN_LSS_MASTER_TX && msg->len >= 1) {
        uint8_t cmd = msg->data[0];
        
        switch (cmd) {
            case CANOPEN_LSS_SWITCH_GLOBAL:
                if (msg->len >= 2) {
                    ctx->lss_state = (msg->data[1] == 1) ? 
                        CANOPEN_LSS_CONFIGURATION : CANOPEN_LSS_WAITING;
                }
                break;
                
            case CANOPEN_LSS_CONFIGURE_NODE_ID:
                /* Slave receives node ID configuration */
                if (msg->len >= 2 && ctx->lss_state == CANOPEN_LSS_CONFIGURATION) {
                    uint8_t new_id = msg->data[1];
                    if (new_id > 0 && (new_id <= 127 || new_id == 255)) {
                        ctx->node_id = new_id;
                    }
                }
                break;
                
            case CANOPEN_LSS_INQUIRE_NODE_ID:
            case CANOPEN_LSS_INQUIRE_VENDOR_ID:
            case CANOPEN_LSS_INQUIRE_PRODUCT_CODE:
            case CANOPEN_LSS_INQUIRE_REVISION:
            case CANOPEN_LSS_INQUIRE_SERIAL:
                /* Slave should respond - handled by get_cyclic or separate response mechanism */
                break;
        }
        return 0;
    }
    
    /* Handle LSS responses (master receives from slave) */
    if (msg->address == CANOPEN_LSS_SLAVE_TX && msg->len >= 5) {
        /* LSS response from slave - decode based on command */
        /* Response format: [CS] [data0] [data1] [data2] [data3] ... */
        /* Could generate events for LSS responses if needed */
        return 0;
    }
    
    /* Handle RPDO (Receive PDO - incoming data) */
    for (int pdo_idx = 0; pdo_idx < 4; pdo_idx++) {
        if (ctx->rpdo[pdo_idx].cob_id == msg->address) {
            /* Decode PDO based on mappings */
            size_t bit_offset = 0;
            
            for (size_t i = 0; i < ctx->rpdo[pdo_idx].num_mappings; i++) {
                const struct lq_canopen_pdo_map *map = &ctx->rpdo[pdo_idx].mappings[i];
                
                if (num_events >= max_events) {
                    break;
                }
                
                /* Extract value from PDO data */
                int32_t value = 0;
                size_t byte_idx = bit_offset / 8;
                size_t length_bytes = (size_t)((map->length + 7U) / 8U);
                
                /* Simple extraction for 8, 16, 32 bit values */
                if (byte_idx + length_bytes <= msg->len) {
                    switch (map->length) {
                        case 8:
                            value = msg->data[byte_idx];
                            break;
                        case 16:
                            value = msg->data[byte_idx] | 
                                   (msg->data[byte_idx + 1] << 8);
                            break;
                        case 32:
                            value = msg->data[byte_idx] | 
                                   (msg->data[byte_idx + 1] << 8) |
                                   (msg->data[byte_idx + 2] << 16) |
                                   (msg->data[byte_idx + 3] << 24);
                            break;
                    }
                    
                    /* Create event */
                    out_events[num_events].source_id = map->signal_id;
                    out_events[num_events].value = value;
                    out_events[num_events].status = LQ_EVENT_OK;
                    out_events[num_events].timestamp = now;
                    num_events++;
                }
                
                bit_offset += map->length;
            }
            
            break;
        }
    }
    
    return num_events;
}

static int canopen_encode(struct lq_protocol_driver *proto,
                          const struct lq_event *events,
                          size_t num_events,
                          struct lq_protocol_msg *out_msg)
{
    /* Encoding happens in get_cyclic or on-demand */
    /* Just update cached signal values here */
    (void)proto;
    (void)events;
    (void)num_events;
    (void)out_msg;
    return -1;
}

static size_t canopen_get_cyclic(struct lq_protocol_driver *proto,
                                  uint64_t now,
                                  struct lq_protocol_msg *out_msgs,
                                  size_t max_msgs)
{
    if (!proto || !out_msgs || max_msgs == 0) {
        return 0;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    size_t num_msgs = 0;
    
    /* Only transmit if in operational state */
    if (ctx->nmt_state != CANOPEN_STATE_OPERATIONAL) {
        return 0;
    }
    
    /* Send heartbeat if due */
    uint64_t elapsed_ms = (now - ctx->last_heartbeat_time) / 1000;
    if (elapsed_ms >= ctx->heartbeat_period_ms && num_msgs < max_msgs) {
        out_msgs[num_msgs].address = lq_canopen_build_cob_id(CANOPEN_FC_HEARTBEAT, ctx->node_id);
        out_msgs[num_msgs].len = 1;
        out_msgs[num_msgs].data[0] = ctx->nmt_state;
        out_msgs[num_msgs].timestamp = now;
        
        ctx->last_heartbeat_time = now;
        num_msgs++;
    }
    
    /* Send emergency if pending */
    if (ctx->emcy_pending && num_msgs < max_msgs) {
        out_msgs[num_msgs].address = lq_canopen_build_cob_id(CANOPEN_FC_EMCY, ctx->node_id);
        out_msgs[num_msgs].len = 8;
        out_msgs[num_msgs].data[0] = (uint8_t)(ctx->emcy_error_code & 0xFFU);
        out_msgs[num_msgs].data[1] = (uint8_t)((ctx->emcy_error_code >> 8) & 0xFFU);
        out_msgs[num_msgs].data[2] = 0;  /* Error register */
        memset(&out_msgs[num_msgs].data[3], 0, 5);  /* Manufacturer data */
        out_msgs[num_msgs].timestamp = now;
        
        ctx->emcy_pending = false;
        num_msgs++;
    }
    
    /* Send TPDOs based on transmission type */
    for (int pdo_idx = 0; pdo_idx < 4 && num_msgs < max_msgs; pdo_idx++) {
        struct lq_canopen_pdo_config *pdo = &ctx->tpdo[pdo_idx];
        
        if (pdo->cob_id == 0) {
            continue;  /* PDO not configured */
        }
        
        /* Check if PDO should be transmitted */
        bool should_transmit = false;
        
        if (pdo->transmission_type == CANOPEN_PDO_EVENT_DRIVEN) {
            /* Event-driven: transmit on change (simplified - check timer) */
            if (pdo->event_timer_ms > 0) {
                /* Timer-based event transmission would go here */
                should_transmit = false;
            }
        } else if (pdo->transmission_type >= CANOPEN_PDO_SYNC_1 &&
                   pdo->transmission_type <= CANOPEN_PDO_SYNC_240) {
            /* SYNC-based: transmit every Nth SYNC */
            if ((ctx->sync_counter % pdo->transmission_type) == 0) {
                should_transmit = true;
            }
        }
        
        if (should_transmit) {
            /* Build PDO message from mapped signals */
            out_msgs[num_msgs].address = pdo->cob_id;
            out_msgs[num_msgs].len = 8;
            out_msgs[num_msgs].timestamp = now;
            memset(out_msgs[num_msgs].data, 0, 8);
            
            size_t bit_offset = 0;
            for (size_t i = 0; i < pdo->num_mappings; i++) {
                const struct lq_canopen_pdo_map *map = &pdo->mappings[i];
                
                /* Find signal value in cache */
                int32_t value = 0;
                for (size_t j = 0; j < ctx->num_signals; j++) {
                    if (ctx->signals[j].signal_id == map->signal_id) {
                        value = ctx->signals[j].value;
                        break;
                    }
                }
                
                /* Pack value into PDO data */
                size_t byte_idx = bit_offset / 8;
                uint32_t uvalue = (uint32_t)value;  /* Convert to unsigned for bitwise ops */
                switch (map->length) {
                    case 8:
                        if (byte_idx < 8) {
                            out_msgs[num_msgs].data[byte_idx] = (uint8_t)uvalue;
                        }
                        break;
                    case 16:
                        if (byte_idx + 1 < 8) {
                            out_msgs[num_msgs].data[byte_idx] = (uint8_t)(uvalue & 0xFFU);
                            out_msgs[num_msgs].data[byte_idx + 1] = (uint8_t)((uvalue >> 8) & 0xFFU);
                        }
                        break;
                    case 32:
                        if (byte_idx + 3 < 8) {
                            out_msgs[num_msgs].data[byte_idx] = (uint8_t)(uvalue & 0xFFU);
                            out_msgs[num_msgs].data[byte_idx + 1] = (uint8_t)((uvalue >> 8) & 0xFFU);
                            out_msgs[num_msgs].data[byte_idx + 2] = (uint8_t)((uvalue >> 16) & 0xFFU);
                            out_msgs[num_msgs].data[byte_idx + 3] = (uint8_t)((uvalue >> 24) & 0xFFU);
                        }
                        break;
                }
                
                bit_offset += map->length;
            }
            
            num_msgs++;
        }
    }
    
    return num_msgs;
}

static void canopen_update_signal(struct lq_protocol_driver *proto,
                                   uint32_t signal_id,
                                   int32_t value,
                                   uint64_t timestamp)
{
    if (!proto) {
        return;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    
    /* Find existing signal or add new */
    for (size_t i = 0; i < ctx->num_signals; i++) {
        if (ctx->signals[i].signal_id == signal_id) {
            ctx->signals[i].value = value;
            ctx->signals[i].timestamp = timestamp;
            return;
        }
    }
    
    /* Add new signal if space available */
    if (ctx->num_signals < 64) {
        ctx->signals[ctx->num_signals].signal_id = signal_id;
        ctx->signals[ctx->num_signals].value = value;
        ctx->signals[ctx->num_signals].timestamp = timestamp;
        ctx->num_signals++;
    }
}

const struct lq_protocol_vtbl lq_canopen_protocol_vtbl = {
    .init = canopen_init,
    .decode = canopen_decode,
    .encode = canopen_encode,
    .get_cyclic = canopen_get_cyclic,
    .update_signal = canopen_update_signal,
};

int lq_canopen_protocol_create(struct lq_protocol_driver *proto,
                                struct lq_canopen_ctx *ctx,
                                const struct lq_protocol_config *config)
{
    if (!proto || !ctx || !config) {
        return -1;
    }
    
    proto->vtbl = &lq_canopen_protocol_vtbl;
    proto->ctx = ctx;
    memcpy(&proto->config, config, sizeof(*config));
    
    return proto->vtbl->init(proto, config);
}

void lq_canopen_set_nmt_state(struct lq_protocol_driver *proto,
                               lq_canopen_nmt_state_t state)
{
    if (!proto || !proto->ctx) {
        return;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    ctx->nmt_state = state;
}

void lq_canopen_send_emergency(struct lq_protocol_driver *proto,
                                uint16_t error_code,
                                uint8_t error_reg,
                                const uint8_t mfr_error[5])
{
    (void)error_reg;  /* Reserved for future use */
    (void)mfr_error;  /* Reserved for future use */
    
    if (!proto || !proto->ctx) {
        return;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    ctx->emcy_error_code = error_code;
    ctx->emcy_pending = true;
}

int lq_canopen_configure_tpdo(struct lq_protocol_driver *proto,
                               uint8_t pdo_num,
                               const struct lq_canopen_pdo_config *config)
{
    if (!proto || !proto->ctx || pdo_num < 1 || pdo_num > 4 || !config) {
        return -1;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    memcpy(&ctx->tpdo[pdo_num - 1], config, sizeof(*config));
    
    return 0;
}

int lq_canopen_configure_rpdo(struct lq_protocol_driver *proto,
                               uint8_t pdo_num,
                               const struct lq_canopen_pdo_config *config)
{
    if (!proto || !proto->ctx || pdo_num < 1 || pdo_num > 4 || !config) {
        return -1;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    memcpy(&ctx->rpdo[pdo_num - 1], config, sizeof(*config));
    
    return 0;
}

/* ============================================================================
 * LSS (Layer Setting Services) Implementation
 * ============================================================================ */

int lq_canopen_set_lss_identity(struct lq_protocol_driver *proto,
                                 uint32_t vendor_id,
                                 uint32_t product_code,
                                 uint32_t revision_number,
                                 uint32_t serial_number)
{
    if (!proto || !proto->ctx) {
        return -1;
    }
    
    struct lq_canopen_ctx *ctx = (struct lq_canopen_ctx *)proto->ctx;
    ctx->vendor_id = vendor_id;
    ctx->product_code = product_code;
    ctx->revision_number = revision_number;
    ctx->serial_number = serial_number;
    
    return 0;
}

int lq_canopen_lss_inquire_node_id(struct lq_protocol_driver *proto,
                                    struct lq_protocol_msg *out_msg)
{
    if (!proto || !out_msg) {
        return -1;
    }
    
    /* Build LSS inquire node ID request */
    out_msg->address = CANOPEN_LSS_MASTER_TX;
    out_msg->len = 8;
    out_msg->data[0] = CANOPEN_LSS_INQUIRE_NODE_ID;
    out_msg->data[1] = 0;
    out_msg->data[2] = 0;
    out_msg->data[3] = 0;
    out_msg->data[4] = 0;
    out_msg->data[5] = 0;
    out_msg->data[6] = 0;
    out_msg->data[7] = 0;
    
    return 0;
}

int lq_canopen_lss_configure_node_id(struct lq_protocol_driver *proto,
                                      uint8_t new_node_id,
                                      struct lq_protocol_msg *out_msg)
{
    if (!proto || !out_msg) {
        return -1;
    }
    
    /* Validate node ID (1-127 valid, 255 = unconfigured) */
    if (new_node_id == 0 || (new_node_id > 127 && new_node_id != 255)) {
        return -1;
    }
    
    /* Build LSS configure node ID command */
    out_msg->address = CANOPEN_LSS_MASTER_TX;
    out_msg->len = 8;
    out_msg->data[0] = CANOPEN_LSS_CONFIGURE_NODE_ID;
    out_msg->data[1] = new_node_id;
    out_msg->data[2] = 0;
    out_msg->data[3] = 0;
    out_msg->data[4] = 0;
    out_msg->data[5] = 0;
    out_msg->data[6] = 0;
    out_msg->data[7] = 0;
    
    return 0;
}

int lq_canopen_lss_switch_state_global(struct lq_protocol_driver *proto,
                                        uint8_t mode,
                                        struct lq_protocol_msg *out_msg)
{
    if (!proto || !out_msg) {
        return -1;
    }
    
    /* Build LSS switch state global command */
    out_msg->address = CANOPEN_LSS_MASTER_TX;
    out_msg->len = 8;
    out_msg->data[0] = CANOPEN_LSS_SWITCH_GLOBAL;
    out_msg->data[1] = mode;  /* 0 = waiting, 1 = configuration */
    out_msg->data[2] = 0;
    out_msg->data[3] = 0;
    out_msg->data[4] = 0;
    out_msg->data[5] = 0;
    out_msg->data[6] = 0;
    out_msg->data[7] = 0;
    
    return 0;
}
