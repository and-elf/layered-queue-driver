/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * CANopen Protocol Support for Layered Queue Driver
 * 
 * Implements CANopen DS-301 protocol with PDO, SDO, and NMT support
 * Uses the unified protocol driver interface (decode RX + encode TX)
 */

#ifndef LQ_CANOPEN_H_
#define LQ_CANOPEN_H_

#include "lq_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CANopen Function Codes (bits 7-11 of COB-ID) */
#define CANOPEN_FC_NMT          0x000   /* Network Management */
#define CANOPEN_FC_SYNC         0x080   /* SYNC */
#define CANOPEN_FC_EMCY         0x080   /* Emergency (node-specific) */
#define CANOPEN_FC_TPDO1        0x180   /* Transmit PDO 1 */
#define CANOPEN_FC_RPDO1        0x200   /* Receive PDO 1 */
#define CANOPEN_FC_TPDO2        0x280   /* Transmit PDO 2 */
#define CANOPEN_FC_RPDO2        0x300   /* Receive PDO 2 */
#define CANOPEN_FC_TPDO3        0x380   /* Transmit PDO 3 */
#define CANOPEN_FC_RPDO3        0x400   /* Receive PDO 3 */
#define CANOPEN_FC_TPDO4        0x480   /* Transmit PDO 4 */
#define CANOPEN_FC_RPDO4        0x500   /* Receive PDO 4 */
#define CANOPEN_FC_TSDO         0x580   /* Transmit SDO (response) */
#define CANOPEN_FC_RSDO         0x600   /* Receive SDO (request) */
#define CANOPEN_FC_HEARTBEAT    0x700   /* Heartbeat */

/* NMT Commands */
typedef enum {
    CANOPEN_NMT_START = 1,              /* Start remote node */
    CANOPEN_NMT_STOP = 2,               /* Stop remote node */
    CANOPEN_NMT_PRE_OPERATIONAL = 128,  /* Enter pre-operational */
    CANOPEN_NMT_RESET_NODE = 129,       /* Reset node */
    CANOPEN_NMT_RESET_COMM = 130,       /* Reset communication */
} lq_canopen_nmt_cmd_t;

/* NMT States */
typedef enum {
    CANOPEN_NMT_BOOTUP = 0,
    CANOPEN_NMT_STOPPED = 4,
    CANOPEN_NMT_OPERATIONAL = 5,
    CANOPEN_NMT_PRE_OPERATIONAL = 127,
} lq_canopen_nmt_state_t;

/* PDO Transmission Types */
typedef enum {
    CANOPEN_PDO_SYNC_ACYCLIC = 0,       /* Triggered by SYNC */
    CANOPEN_PDO_SYNC_1 = 1,             /* Every 1st SYNC */
    CANOPEN_PDO_SYNC_240 = 240,         /* Every 240th SYNC */
    CANOPEN_PDO_EVENT_DRIVEN = 254,     /* Event-driven (on change) */
    CANOPEN_PDO_EVENT_MANUFACTURER = 255,/* Manufacturer-specific */
} lq_canopen_pdo_type_t;

/* SDO Command Specifiers */
#define CANOPEN_SDO_DOWNLOAD_INIT       0x20
#define CANOPEN_SDO_DOWNLOAD_SEGMENT    0x00
#define CANOPEN_SDO_UPLOAD_INIT         0x40
#define CANOPEN_SDO_UPLOAD_SEGMENT      0x60
#define CANOPEN_SDO_ABORT               0x80

/* Emergency Error Codes */
#define CANOPEN_EMCY_NO_ERROR           0x0000
#define CANOPEN_EMCY_GENERIC            0x1000
#define CANOPEN_EMCY_CURRENT            0x2000
#define CANOPEN_EMCY_VOLTAGE            0x3100
#define CANOPEN_EMCY_TEMPERATURE        0x4000
#define CANOPEN_EMCY_COMMUNICATION      0x8100
#define CANOPEN_EMCY_DEVICE_SPECIFIC    0xFF00

/* PDO Mapping Entry */
struct lq_canopen_pdo_map {
    uint16_t index;                     /* Object dictionary index */
    uint8_t subindex;                   /* Object dictionary subindex */
    uint8_t length;                     /* Data length in bits */
    uint32_t signal_id;                 /* LQ signal ID */
};

/* PDO Configuration */
struct lq_canopen_pdo_config {
    uint16_t cob_id;                    /* COB-ID for this PDO */
    uint8_t transmission_type;          /* PDO transmission type */
    uint16_t event_timer_ms;            /* Event timer in ms */
    uint16_t inhibit_time_100us;        /* Inhibit time in 100us */
    
    struct lq_canopen_pdo_map mappings[8];  /* Up to 8 mappings per PDO */
    size_t num_mappings;
};

/* CANopen Protocol Driver Context */
struct lq_canopen_ctx {
    uint8_t node_id;                    /* Our node ID (1-127) */
    lq_canopen_nmt_state_t nmt_state;   /* Current NMT state */
    
    /* PDO configurations */
    struct lq_canopen_pdo_config tpdo[4];  /* Transmit PDOs */
    struct lq_canopen_pdo_config rpdo[4];  /* Receive PDOs */
    
    /* Cached signal values for TX */
    struct {
        uint32_t signal_id;
        int32_t value;
        uint64_t timestamp;
    } signals[64];
    size_t num_signals;
    
    /* SYNC counter */
    uint8_t sync_counter;
    
    /* Heartbeat timing */
    uint16_t heartbeat_period_ms;
    uint64_t last_heartbeat_time;
    
    /* Emergency state */
    uint16_t emcy_error_code;
    bool emcy_pending;
};

/**
 * @brief Create CANopen protocol driver instance
 * 
 * @param proto Protocol driver to initialize
 * @param config Protocol configuration (node ID, PDO mappings, etc)
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_protocol_create(struct lq_protocol_driver *proto,
                                const struct lq_protocol_config *config);

/**
 * @brief CANopen Protocol Driver vtable
 * 
 * Exported for advanced users who want to customize behavior.
 */
extern const struct lq_protocol_vtbl lq_canopen_protocol_vtbl;

/**
 * @brief Set NMT state
 * 
 * @param proto CANopen protocol driver
 * @param state New NMT state
 */
void lq_canopen_set_nmt_state(struct lq_protocol_driver *proto,
                               lq_canopen_nmt_state_t state);

/**
 * @brief Trigger emergency message
 * 
 * @param proto CANopen protocol driver
 * @param error_code Emergency error code
 * @param error_reg Error register value
 * @param mfr_error Manufacturer-specific error data
 */
void lq_canopen_send_emergency(struct lq_protocol_driver *proto,
                                uint16_t error_code,
                                uint8_t error_reg,
                                const uint8_t mfr_error[5]);

/**
 * @brief Configure TPDO
 * 
 * @param proto CANopen protocol driver
 * @param pdo_num PDO number (1-4)
 * @param config PDO configuration
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_configure_tpdo(struct lq_protocol_driver *proto,
                               uint8_t pdo_num,
                               const struct lq_canopen_pdo_config *config);

/**
 * @brief Configure RPDO
 * 
 * @param proto CANopen protocol driver
 * @param pdo_num PDO number (1-4)
 * @param config PDO configuration
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_configure_rpdo(struct lq_protocol_driver *proto,
                               uint8_t pdo_num,
                               const struct lq_canopen_pdo_config *config);

/* Utility functions for COB-ID construction */
static inline uint16_t lq_canopen_build_cob_id(uint16_t function_code, uint8_t node_id)
{
    return function_code + node_id;
}

static inline uint8_t lq_canopen_get_node_id(uint16_t cob_id, uint16_t function_code)
{
    return (uint8_t)(cob_id - function_code);
}

#ifdef __cplusplus
}
#endif

#endif /* LQ_CANOPEN_H_ */
