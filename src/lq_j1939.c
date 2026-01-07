/*
 * J1939 Protocol Implementation
 * Implements the unified protocol driver interface
 */

#include "lq_j1939.h"
#include <string.h>
#include <stdlib.h>

/* J1939 Protocol Driver Implementation */

static int j1939_init(struct lq_protocol_driver *proto, 
                      const struct lq_protocol_config *config)
{
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)proto->ctx;
    
    if (!ctx || !config) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->node_address = config->node_address;
    
    /* Initialize cyclic message tracking from encode maps */
    for (size_t i = 0; i < config->num_encode_maps && i < 16; i++) {
        ctx->cyclic_msgs[i].pgn = config->encode_maps[i].protocol_id;
        ctx->cyclic_msgs[i].period_ms = config->encode_maps[i].period_ms;
        ctx->cyclic_msgs[i].last_tx_time = 0;
    }
    ctx->num_cyclic = config->num_encode_maps < 16 ? config->num_encode_maps : 16;
    
    return 0;
}

static size_t j1939_decode(struct lq_protocol_driver *proto,
                           uint64_t now,
                           const struct lq_protocol_msg *msg,
                           struct lq_event *out_events,
                           size_t max_events)
{
    if (!proto || !msg || !out_events || max_events == 0) {
        return 0;
    }
    
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)proto->ctx;
    uint32_t pgn = lq_j1939_extract_pgn(msg->address);
    size_t num_events = 0;
    
    if (!msg->data || msg->len < 8) {
        return 0;  /* J1939 requires 8-byte CAN frames */
    }
    
    /* Find decode mapping for this PGN */
    const struct lq_protocol_decode_map *map = NULL;
    for (size_t i = 0; i < proto->config.num_decode_maps; i++) {
        if (proto->config.decode_maps[i].protocol_id == pgn) {
            map = &proto->config.decode_maps[i];
            break;
        }
    }
    
    if (!map) {
        return 0;  /* PGN not configured for decode */
    }
    
    /* Decode based on PGN */
    switch (pgn) {
        case J1939_PGN_EEC1: {
            /* Engine speed: bytes 3-4, 0.125 rpm/bit */
            uint16_t raw_rpm = msg->data[3] | (msg->data[4] << 8);
            if (num_events < max_events && map->num_signals > 0) {
                out_events[num_events].source_id = map->signal_ids[0];
                out_events[num_events].value = (int32_t)(raw_rpm * 0.125);
                out_events[num_events].status = LQ_EVENT_OK;
                out_events[num_events].timestamp = now;
                num_events++;
            }
            
            /* Engine torque: byte 2, 1% per bit, offset -125 */
            if (num_events < max_events && map->num_signals > 1) {
                out_events[num_events].source_id = map->signal_ids[1];
                out_events[num_events].value = (int32_t)msg->data[2] - 125;
                out_events[num_events].status = LQ_EVENT_OK;
                out_events[num_events].timestamp = now;
                num_events++;
            }
            break;
        }
        
        case J1939_PGN_ET1: {
            /* Engine coolant temp: byte 0, 1°C/bit, offset -40 */
            if (num_events < max_events && map->num_signals > 0) {
                out_events[num_events].source_id = map->signal_ids[0];
                out_events[num_events].value = (int32_t)msg->data[0] - 40;
                out_events[num_events].status = LQ_EVENT_OK;
                out_events[num_events].timestamp = now;
                num_events++;
            }
            break;
        }
        
        /* Add more PGN decoders as needed */
    }
    
    return num_events;
}

static int j1939_encode(struct lq_protocol_driver *proto,
                        const struct lq_event *events,
                        size_t num_events,
                        struct lq_protocol_msg *out_msg)
{
    if (!proto || !events || !out_msg || num_events == 0) {
        return -1;
    }
    
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)proto->ctx;
    
    /* Update cached signal values */
    for (size_t i = 0; i < num_events; i++) {
        for (size_t j = 0; j < ctx->num_signals; j++) {
            if (ctx->signals[j].signal_id == events[i].source_id) {
                ctx->signals[j].value = events[i].value;
                ctx->signals[j].timestamp = events[i].timestamp;
                break;
            }
        }
    }
    
    /* For now, return -1 to indicate encoding should happen in get_cyclic */
    return -1;
}

static size_t j1939_get_cyclic(struct lq_protocol_driver *proto,
                                uint64_t now,
                                struct lq_protocol_msg *out_msgs,
                                size_t max_msgs)
{
    if (!proto || !out_msgs || max_msgs == 0) {
        return 0;
    }
    
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)proto->ctx;
    size_t num_msgs = 0;
    
    /* Check each cyclic message */
    for (size_t i = 0; i < ctx->num_cyclic && num_msgs < max_msgs; i++) {
        uint64_t elapsed_ms = (now - ctx->cyclic_msgs[i].last_tx_time) / 1000;
        
        if (elapsed_ms >= ctx->cyclic_msgs[i].period_ms) {
            uint32_t pgn = ctx->cyclic_msgs[i].pgn;
            
            /* Build message based on PGN */
            out_msgs[num_msgs].address = lq_j1939_build_id_from_pgn(pgn, 6, ctx->node_address);
            out_msgs[num_msgs].len = 8;
            out_msgs[num_msgs].timestamp = now;
            memset(out_msgs[num_msgs].data, 0xFF, 8);  /* J1939 default */
            
            switch (pgn) {
                case J1939_PGN_EEC1: {
                    /* Find RPM and torque signals from cached values */
                    /* This is simplified - real implementation would map signal IDs */
                    if (ctx->num_signals >= 2) {
                        uint16_t rpm = (uint16_t)(ctx->signals[0].value / 0.125);
                        int8_t torque = (int8_t)(ctx->signals[1].value + 125);
                        
                        out_msgs[num_msgs].data[2] = (uint8_t)torque;
                        out_msgs[num_msgs].data[3] = rpm & 0xFF;
                        out_msgs[num_msgs].data[4] = (rpm >> 8) & 0xFF;
                    }
                    break;
                }
                
                case J1939_PGN_DM1: {
                    /* Encode DM1 from diagnostic state */
                    lq_j1939_format_dm1(&ctx->dm1, out_msgs[num_msgs].data, 8);
                    break;
                }
            }
            
            ctx->cyclic_msgs[i].last_tx_time = now;
            num_msgs++;
        }
    }
    
    return num_msgs;
}

static void j1939_update_signal(struct lq_protocol_driver *proto,
                                 uint32_t signal_id,
                                 int32_t value,
                                 uint64_t timestamp)
{
    if (!proto) {
        return;
    }
    
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)proto->ctx;
    
    /* Find existing signal or add new */
    for (size_t i = 0; i < ctx->num_signals; i++) {
        if (ctx->signals[i].signal_id == signal_id) {
            ctx->signals[i].value = value;
            ctx->signals[i].timestamp = timestamp;
            return;
        }
    }
    
    /* Add new signal if space available */
    if (ctx->num_signals < 32) {
        ctx->signals[ctx->num_signals].signal_id = signal_id;
        ctx->signals[ctx->num_signals].value = value;
        ctx->signals[ctx->num_signals].timestamp = timestamp;
        ctx->num_signals++;
    }
}

const struct lq_protocol_vtbl lq_j1939_protocol_vtbl = {
    .init = j1939_init,
    .decode = j1939_decode,
    .encode = j1939_encode,
    .get_cyclic = j1939_get_cyclic,
    .update_signal = j1939_update_signal,
};

int lq_j1939_protocol_create(struct lq_protocol_driver *proto,
                              const struct lq_protocol_config *config)
{
    if (!proto) {
        return -1;
    }
    
    /* Allocate context */
    struct lq_j1939_ctx *ctx = (struct lq_j1939_ctx *)malloc(sizeof(struct lq_j1939_ctx));
    if (!ctx) {
        return -1;
    }
    
    proto->vtbl = &lq_j1939_protocol_vtbl;
    proto->ctx = ctx;
    memcpy(&proto->config, config, sizeof(*config));
    
    return proto->vtbl->init(proto, config);
}

/* Legacy utility functions */

int lq_j1939_format_dm1(const lq_j1939_dm1_t *dm1, uint8_t *data, size_t data_len)
{
    if (!dm1 || !data || data_len < 8) {
        return -1;
    }
    
    memset(data, 0xFF, data_len);  /* J1939 default: 0xFF = not available */
    
    /* Byte 0-1: Lamp status */
    data[0] = (dm1->protect_lamp & 0x03) |
              ((dm1->amber_warning_lamp & 0x03) << 2) |
              ((dm1->red_stop_lamp & 0x03) << 4) |
              ((dm1->malfunction_lamp & 0x03) << 6);
    
    data[1] = (dm1->flash_malfunction_lamp & 0x03) |
              ((dm1->flash_red_stop_lamp & 0x03) << 2) |
              ((dm1->flash_amber_warning_lamp & 0x03) << 4) |
              ((dm1->flash_protect_lamp & 0x03) << 6);
    
    /* Bytes 2-7: First DTC (if any) */
    if (dm1->dtc_count > 0) {
        uint32_t dtc = dm1->dtc_list[0];
        uint32_t spn = lq_j1939_get_spn(dtc);
        uint8_t fmi = lq_j1939_get_fmi(dtc);
        uint8_t oc = lq_j1939_get_oc(dtc);
        
        /* SPN: 19 bits split across bytes 2-4 */
        data[2] = spn & 0xFF;
        data[3] = (spn >> 8) & 0xFF;
        data[4] = ((spn >> 16) & 0x07) | ((fmi & 0x1F) << 3);
        data[5] = ((fmi >> 5) & 0x03) | (oc << 2);
        
        /* Conversion method (always 0 for J1939) */
        data[6] = 0;
        data[7] = 0;
    }
    
    return 0;
}

int lq_j1939_format_dm0(lq_j1939_lamp_t stop_lamp, lq_j1939_lamp_t warning_lamp,
                        uint8_t *data, size_t data_len)
{
    if (!data || data_len < 8) {
        return -1;
    }
    
    memset(data, 0xFF, data_len);
    
    /* DM0 format similar to DM1 but without DTCs */
    data[0] = ((stop_lamp & 0x03) << 4) |
              ((warning_lamp & 0x03) << 2);
    
    data[1] = 0xFF;  /* No flash patterns */
    
    return 0;
}

int lq_j1939_decode_eec1(const uint8_t *data, uint16_t *rpm, uint8_t *torque)
{
    if (!data || !rpm || !torque) {
        return -1;
    }
    
    /* Engine speed: bytes 3-4, 0.125 rpm/bit */
    uint16_t raw_rpm = data[3] | (data[4] << 8);
    *rpm = (uint16_t)(raw_rpm * 0.125);
    
    /* Engine torque: byte 2, 1% per bit, offset -125 */
    *torque = data[2];
    
    return 0;
}

int lq_j1939_encode_eec1(uint16_t rpm, uint8_t torque, uint8_t *data)
{
    if (!data) {
        return -1;
    }
    
    memset(data, 0xFF, 8);
    
    /* Torque: byte 2 */
    data[2] = torque;
    
    /* Engine speed: bytes 3-4 */
    uint16_t raw_rpm = (uint16_t)(rpm / 0.125);
    data[3] = raw_rpm & 0xFF;
    data[4] = (raw_rpm >> 8) & 0xFF;
    
    return 0;
}

int lq_j1939_decode_dm1(const uint8_t *data, size_t len, lq_j1939_dm1_t *dm1)
{
    if (!data || !dm1 || len < 8) {
        return -1;
    }
    
    memset(dm1, 0, sizeof(*dm1));
    
    /* Byte 0: Lamp status */
    dm1->protect_lamp = (lq_j1939_lamp_t)(data[0] & 0x03);
    dm1->amber_warning_lamp = (lq_j1939_lamp_t)((data[0] >> 2) & 0x03);
    dm1->red_stop_lamp = (lq_j1939_lamp_t)((data[0] >> 4) & 0x03);
    dm1->malfunction_lamp = (lq_j1939_lamp_t)((data[0] >> 6) & 0x03);
    
    /* Byte 1: Flash patterns */
    dm1->flash_malfunction_lamp = data[1] & 0x03;
    dm1->flash_red_stop_lamp = (data[1] >> 2) & 0x03;
    dm1->flash_amber_warning_lamp = (data[1] >> 4) & 0x03;
    dm1->flash_protect_lamp = (data[1] >> 6) & 0x03;
    
    /* Bytes 2-7: First DTC */
    if (len >= 8) {
        uint32_t spn = data[2] | (data[3] << 8) | ((data[4] & 0x07) << 16);
        uint8_t fmi = ((data[4] >> 3) & 0x1F) | ((data[5] & 0x03) << 5);
        uint8_t oc = (data[5] >> 2) & 0x7F;
        
        if (spn != 0x7FFFF) {  /* Not "not available" */
            dm1->dtc_list[0] = lq_j1939_create_dtc(spn, fmi, oc);
            dm1->dtc_count = 1;
        }
    }
    
    return 0;
}
