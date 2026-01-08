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

/* LSS (Layer Setting Services) COB-IDs */
#define CANOPEN_LSS_MASTER_TX   0x7E5   /* LSS Master -> Slave */
#define CANOPEN_LSS_SLAVE_TX    0x7E4   /* LSS Slave -> Master */

/* LSS Command Specifiers */
#define CANOPEN_LSS_SWITCH_GLOBAL       0x04  /* Switch state global */
#define CANOPEN_LSS_SWITCH_SELECTIVE    0x40  /* Switch state selective */
#define CANOPEN_LSS_CONFIGURE_NODE_ID   0x11  /* Configure node ID */
#define CANOPEN_LSS_CONFIGURE_BIT_TIMING 0x13 /* Configure bit timing */
#define CANOPEN_LSS_ACTIVATE_BIT_TIMING 0x15  /* Activate bit timing */
#define CANOPEN_LSS_STORE_CONFIG        0x17  /* Store configuration */
#define CANOPEN_LSS_INQUIRE_VENDOR_ID   0x5A  /* Inquire vendor ID */
#define CANOPEN_LSS_INQUIRE_PRODUCT_CODE 0x5B /* Inquire product code */
#define CANOPEN_LSS_INQUIRE_REVISION    0x5C  /* Inquire revision */
#define CANOPEN_LSS_INQUIRE_SERIAL      0x5D  /* Inquire serial number */
#define CANOPEN_LSS_INQUIRE_NODE_ID     0x5E  /* Inquire node ID */
#define CANOPEN_LSS_IDENTIFY_REMOTE_SLAVE 0x46 /* Identify remote slave */
#define CANOPEN_LSS_IDENTIFY_NON_CONFIGURED 0x50 /* Identify non-configured slaves */

/* LSS States */
typedef enum {
    CANOPEN_LSS_WAITING = 0,            /* Waiting state */
    CANOPEN_LSS_CONFIGURATION = 1,      /* Configuration state */
} lq_canopen_lss_state_t;

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
    CANOPEN_STATE_BOOTUP = 0,
    CANOPEN_STATE_STOPPED = 4,
    CANOPEN_STATE_OPERATIONAL = 5,
    CANOPEN_STATE_PRE_OPERATIONAL = 127,
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
    
    /* LSS state and identity */
    lq_canopen_lss_state_t lss_state;   /* LSS state */
    uint32_t vendor_id;                 /* Vendor ID */
    uint32_t product_code;              /* Product code */
    uint32_t revision_number;           /* Revision number */
    uint32_t serial_number;             /* Serial number */
    
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
 * @param ctx CANopen context (caller-provided, no malloc)
 * @param config Protocol configuration (node ID, PDO mappings, etc)
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_protocol_create(struct lq_protocol_driver *proto,
                                struct lq_canopen_ctx *ctx,
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

/**
 * @brief Set LSS identity information
 * 
 * Used for LSS inquire commands and slave identification.
 * 
 * @param proto CANopen protocol driver
 * @param vendor_id Vendor ID
 * @param product_code Product code
 * @param revision_number Revision number
 * @param serial_number Serial number
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_set_lss_identity(struct lq_protocol_driver *proto,
                                 uint32_t vendor_id,
                                 uint32_t product_code,
                                 uint32_t revision_number,
                                 uint32_t serial_number);

/**
 * @brief Send LSS inquire node ID request (Master function)
 * 
 * Requests the current node ID from a slave in LSS configuration state.
 * Response will be received via the protocol decode callback.
 * 
 * @param proto CANopen protocol driver
 * @param out_msg Output message buffer
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_lss_inquire_node_id(struct lq_protocol_driver *proto,
                                    struct lq_protocol_msg *out_msg);

/**
 * @brief Send LSS configure node ID command (Master function)
 * 
 * Configures the node ID of a slave in LSS configuration state.
 * 
 * @param proto CANopen protocol driver
 * @param new_node_id New node ID (1-127, or 255 for unconfigured)
 * @param out_msg Output message buffer
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_lss_configure_node_id(struct lq_protocol_driver *proto,
                                      uint8_t new_node_id,
                                      struct lq_protocol_msg *out_msg);

/**
 * @brief Send LSS switch state global command (Master function)
 * 
 * Switches all slaves between waiting and configuration state.
 * 
 * @param proto CANopen protocol driver
 * @param mode 0 = waiting state, 1 = configuration state
 * @param out_msg Output message buffer
 * @return 0 on success, negative errno on failure
 */
int lq_canopen_lss_switch_state_global(struct lq_protocol_driver *proto,
                                        uint8_t mode,
                                        struct lq_protocol_msg *out_msg);

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
