/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Zephyr Device Tree Wrapper for J1939 Protocol Driver
 */

#define DT_DRV_COMPAT lq_protocol_j1939

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "lq_j1939.h"
#include "lq_protocol.h"

LOG_MODULE_REGISTER(lq_protocol_j1939, CONFIG_LQ_LOG_LEVEL);

/* Device tree data structures */
struct lq_j1939_dt_data {
    struct lq_j1939_ctx ctx;
    uint8_t *rx_buffer;
    uint8_t *tx_buffer;
};

struct lq_j1939_dt_config {
    struct lq_protocol_driver driver;
    struct lq_protocol_config protocol_config;
    const struct lq_protocol_decode_map *decode_maps;
    const struct lq_protocol_encode_map *encode_maps;
    size_t num_decode_maps;
    size_t num_encode_maps;
    size_t mtu;
};

/* Device API placeholder - protocols are used via lq_protocol_driver interface */
static const struct device_api lq_j1939_api = {
    /* Protocol drivers accessed through proto->vtbl, not device API */
};

static int lq_j1939_dt_init(const struct device *dev)
{
    struct lq_j1939_dt_data *data = dev->data;
    const struct lq_j1939_dt_config *config = dev->config;
    
    /* Initialize protocol driver */
    struct lq_protocol_driver *proto = (struct lq_protocol_driver *)&config->driver;
    proto->ctx = &data->ctx;
    
    int ret = lq_protocol_init(proto, &config->protocol_config);
    if (ret != 0) {
        LOG_ERR("J1939 init failed: %d", ret);
        return ret;
    }
    
    LOG_INF("J1939 protocol initialized: node=0x%02x, mtu=%zu",
            config->protocol_config.node_address, config->mtu);
    
    return 0;
}

/* Helper macro to get child node count */
#define CHILD_NODE_COUNT(node_id) \
    DT_FOREACH_CHILD_STATUS_OKAY_SEP(node_id, (+), (1))

/* Count RX children (direction="rx" or "both") */
#define IS_RX_CHILD(child) \
    (DT_PROP(child, direction) == 0 /* rx */ || DT_PROP(child, direction) == 2 /* both */)

#define IS_TX_CHILD(child) \
    (DT_PROP(child, direction) == 1 /* tx */ || DT_PROP(child, direction) == 2 /* both */)

/* Macro to define a decode map from child node */
#define J1939_DECODE_MAP_INIT(child) \
    { \
        .protocol_id = DT_PROP(child, pgn), \
        .signal_ids = (uint32_t[]){ DT_FOREACH_PROP_ELEM_SEP(child, signal_ids, DT_PROP_BY_IDX, (,)) }, \
        .num_signals = DT_PROP_LEN(child, signal_ids), \
        .user_data = NULL, \
    },

/* Macro to define an encode map from child node */
#define J1939_ENCODE_MAP_INIT(child) \
    { \
        .protocol_id = DT_PROP(child, pgn), \
        .signal_ids = (uint32_t[]){ DT_FOREACH_PROP_ELEM_SEP(child, signal_ids, DT_PROP_BY_IDX, (,)) }, \
        .num_signals = DT_PROP_LEN(child, signal_ids), \
        .period_ms = DT_PROP_OR(child, period_ms, 0), \
        .on_change = DT_PROP_OR(child, on_change, false), \
        .user_data = NULL, \
    },

/* Main device instantiation macro */
#define LQ_PROTOCOL_J1939_DEFINE(inst) \
    /* Static RX/TX buffers sized from DT mtu property */ \
    static uint8_t lq_j1939_rx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    static uint8_t lq_j1939_tx_buf_##inst[DT_INST_PROP(inst, mtu)]; \
    \
    /* Decode maps from child nodes with direction="rx" or "both" */ \
    static const struct lq_protocol_decode_map lq_j1939_decode_maps_##inst[] = { \
        DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(inst, J1939_DECODE_MAP_INIT) \
    }; \
    \
    /* Encode maps from child nodes with direction="tx" or "both" */ \
    static const struct lq_protocol_encode_map lq_j1939_encode_maps_##inst[] = { \
        DT_INST_FOREACH_CHILD_STATUS_OKAY_VARGS(inst, J1939_ENCODE_MAP_INIT) \
    }; \
    \
    /* Device data */ \
    static struct lq_j1939_dt_data lq_j1939_data_##inst = { \
        .rx_buffer = lq_j1939_rx_buf_##inst, \
        .tx_buffer = lq_j1939_tx_buf_##inst, \
    }; \
    \
    /* Device config */ \
    static const struct lq_j1939_dt_config lq_j1939_config_##inst = { \
        .driver = { \
            .vtbl = &lq_j1939_protocol_vtbl, \
            .ctx = NULL, /* Set during init */ \
        }, \
        .protocol_config = { \
            .node_address = DT_INST_PROP(inst, node_address), \
            .decode_maps = lq_j1939_decode_maps_##inst, \
            .num_decode_maps = ARRAY_SIZE(lq_j1939_decode_maps_##inst), \
            .encode_maps = lq_j1939_encode_maps_##inst, \
            .num_encode_maps = ARRAY_SIZE(lq_j1939_encode_maps_##inst), \
        }, \
        .decode_maps = lq_j1939_decode_maps_##inst, \
        .encode_maps = lq_j1939_encode_maps_##inst, \
        .num_decode_maps = ARRAY_SIZE(lq_j1939_decode_maps_##inst), \
        .num_encode_maps = ARRAY_SIZE(lq_j1939_encode_maps_##inst), \
        .mtu = DT_INST_PROP(inst, mtu), \
    }; \
    \
    DEVICE_DT_INST_DEFINE(inst, \
                          lq_j1939_dt_init, \
                          NULL, \
                          &lq_j1939_data_##inst, \
                          &lq_j1939_config_##inst, \
                          POST_KERNEL, \
                          CONFIG_LQ_INIT_PRIORITY, \
                          &lq_j1939_api);

/* Instantiate all J1939 protocol instances from device tree */
DT_INST_FOREACH_STATUS_OKAY(LQ_PROTOCOL_J1939_DEFINE)
