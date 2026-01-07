/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Tree Wrapper for CANopen Protocol Driver
 */

#define DT_DRV_COMPAT lq_protocol_canopen

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "lq_canopen.h"
#include "lq_protocol.h"

LOG_MODULE_REGISTER(lq_protocol_canopen, CONFIG_LQ_LOG_LEVEL);

/* Device tree data structures */
struct lq_canopen_dt_data {
    struct lq_canopen_ctx ctx;
    uint8_t *rx_buffer;
    uint8_t *tx_buffer;
};

struct lq_canopen_dt_config {
    struct lq_protocol_driver driver;
    struct lq_protocol_config protocol_config;
    const struct lq_protocol_decode_map *decode_maps;
    const struct lq_protocol_encode_map *encode_maps;
    size_t num_decode_maps;
    size_t num_encode_maps;
    size_t mtu;
    uint32_t heartbeat_period_ms;
};

/* Device API placeholder - protocols are used via lq_protocol_driver interface */
static const struct device_api lq_canopen_api = {
    /* Protocol drivers accessed through proto->vtbl, not device API */
};

static int lq_canopen_dt_init(const struct device *dev)
{
    struct lq_canopen_dt_data *data = dev->data;
    const struct lq_canopen_dt_config *config = dev->config;
    
    /* Initialize protocol driver */
    struct lq_protocol_driver *proto = (struct lq_protocol_driver *)&config->driver;
    proto->ctx = &data->ctx;
    
    int ret = lq_protocol_init(proto, &config->protocol_config);
    if (ret != 0) {
        LOG_ERR("CANopen init failed: %d", ret);
        return ret;
    }
    
    LOG_INF("CANopen protocol initialized: node=%u, mtu=%zu, heartbeat=%ums",
            config->protocol_config.node_address, config->mtu,
            config->heartbeat_period_ms);
    
    return 0;
}

/* Macro to define a decode map (RPDO) from child node */
#define CANOPEN_DECODE_MAP_INIT(child) \
    { \
        .protocol_id = DT_PROP(child, cob_id), \
        .signal_ids = (uint32_t[]){ DT_FOREACH_PROP_ELEM_SEP(child, signal_ids, DT_PROP_BY_IDX, (,)) }, \
        .num_signals = DT_PROP_LEN(child, signal_ids), \
        .user_data = NULL, \
    },

/* Macro to define an encode map (TPDO) from child node */
#define CANOPEN_ENCODE_MAP_INIT(child) \
    { \
        .protocol_id = DT_PROP(child, cob_id), \
        .signal_ids = (uint32_t[]){ DT_FOREACH_PROP_ELEM_SEP(child, signal_ids, DT_PROP_BY_IDX, (,)) }, \
        .num_signals = DT_PROP_LEN(child, signal_ids), \
        .period_ms = DT_PROP_OR(child, event_timer_ms, 0), \
        .on_change = (DT_PROP_OR(child, transmission_type, 0) >= 254), \
        .user_data = NULL, \
    },

/* Main device instantiation macro */
#define LQ_PROTOCOL_CANOPEN_DEFINE(inst) \
    /* Static RX/TX buffers sized from DT mtu property */ \
    static uint8_t lq_canopen_rx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    static uint8_t lq_canopen_tx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    \
    /* Decode maps from RPDO child nodes */ \
    static const struct lq_protocol_decode_map lq_canopen_decode_maps_##inst[] = { \
        DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(inst, CANOPEN_DECODE_MAP_INIT) \
    }; \
    \
    /* Encode maps from TPDO child nodes */ \
    static const struct lq_protocol_encode_map lq_canopen_encode_maps_##inst[] = { \
        DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(inst, CANOPEN_ENCODE_MAP_INIT) \
    }; \
    \
    /* Device data */ \
    static struct lq_canopen_dt_data lq_canopen_data_##inst = { \
        .rx_buffer = lq_canopen_rx_buf_##inst, \
        .tx_buffer = lq_canopen_tx_buf_##inst, \
    }; \
    \
    /* Device config */ \
    static const struct lq_canopen_dt_config lq_canopen_config_##inst = { \
        .driver = { \
            .vtbl = &lq_canopen_protocol_vtbl, \
            .ctx = NULL, /* Set during init */ \
        }, \
        .protocol_config = { \
            .node_address = DT_INST_PROP(inst, node_id), \
            .decode_maps = lq_canopen_decode_maps_##inst, \
            .num_decode_maps = ARRAY_SIZE(lq_canopen_decode_maps_##inst), \
            .encode_maps = lq_canopen_encode_maps_##inst, \
            .num_encode_maps = ARRAY_SIZE(lq_canopen_encode_maps_##inst), \
        }, \
        .decode_maps = lq_canopen_decode_maps_##inst, \
        .encode_maps = lq_canopen_encode_maps_##inst, \
        .num_decode_maps = ARRAY_SIZE(lq_canopen_decode_maps_##inst), \
        .num_encode_maps = ARRAY_SIZE(lq_canopen_encode_maps_##inst), \
        .mtu = DT_INST_PROP(inst, mtu), \
        .heartbeat_period_ms = DT_INST_PROP_OR(inst, heartbeat_period_ms, 1000), \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_canopen_dt_init, \
                          NULL, \
                          &lq_canopen_data_##inst, \
                          &lq_canopen_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_LQ_INIT_PRIORITY, \
                          &lq_canopen_api);

/* Instantiate all CANopen protocol instances from device tree */
DT_INST_FOREACH_STATUS_OKAY(LQ_PROTOCOL_CANOPEN_DEFINE)
