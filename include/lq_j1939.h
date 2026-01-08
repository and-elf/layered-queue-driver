/*
 * J1939 Protocol Support for Layered Queue Driver
 * 
 * Provides J1939 message formatting and diagnostic protocol support
 * Implements the unified protocol driver interface (decode RX + encode TX)
 */

#ifndef LQ_J1939_H_
#define LQ_J1939_H_

#include "lq_protocol.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* J1939 Standard PGNs */
#define J1939_PGN_EEC1          65265  /* Electronic Engine Controller 1 (0xFEF1) */
#define J1939_PGN_EEC2          65266  /* Electronic Engine Controller 2 (0xFEF2) */
#define J1939_PGN_ET1           65262  /* Engine Temperature 1 (0xFEEE) */
#define J1939_PGN_EFL_P1        65263  /* Engine Fluid Level/Pressure 1 (0xFEEF) */
#define J1939_PGN_DM1           65226  /* Active Diagnostic Trouble Codes (0xFECA) */
#define J1939_PGN_DM2           65227  /* Previously Active DTCs (0xFECB) */
#define J1939_PGN_DM13          57088  /* Stop Start Broadcast (0xDF00) */
#define J1939_PGN_REQUEST       59904  /* Request PGN (0xEA00) */

/* J1939 Diagnostic Lamp Status */
typedef enum {
    J1939_LAMP_OFF = 0,
    J1939_LAMP_ON = 1,
    J1939_LAMP_SLOW_FLASH = 2,
    J1939_LAMP_FAST_FLASH = 3,
} lq_j1939_lamp_t;

/* J1939 DM1 Message Structure */
typedef struct {
    lq_j1939_lamp_t malfunction_lamp;      /* MIL (Malfunction Indicator Lamp) */
    lq_j1939_lamp_t red_stop_lamp;         /* Red stop lamp */
    lq_j1939_lamp_t amber_warning_lamp;    /* Amber warning lamp */
    lq_j1939_lamp_t protect_lamp;          /* Protect lamp */
    
    uint16_t flash_malfunction_lamp;
    uint16_t flash_red_stop_lamp;
    uint16_t flash_amber_warning_lamp;
    uint16_t flash_protect_lamp;
    
    /* DTCs (max 255, but typically 1-10 fit in single message) */
    uint32_t dtc_list[10];  /* SPN (19 bits) | FMI (5 bits) | OC (7 bits) */
    uint8_t dtc_count;
} lq_j1939_dm1_t;

/* J1939 Failure Mode Indicator (FMI) */
typedef enum {
    J1939_FMI_DATA_ABOVE_NORMAL = 0,
    J1939_FMI_DATA_BELOW_NORMAL = 1,
    J1939_FMI_DATA_ERRATIC = 2,
    J1939_FMI_VOLTAGE_ABOVE_NORMAL = 3,
    J1939_FMI_VOLTAGE_BELOW_NORMAL = 4,
    J1939_FMI_CURRENT_BELOW_NORMAL = 5,
    J1939_FMI_CURRENT_ABOVE_NORMAL = 6,
    J1939_FMI_MECHANICAL_FAILURE = 7,
    J1939_FMI_ABNORMAL_FREQUENCY = 8,
    J1939_FMI_ABNORMAL_UPDATE_RATE = 9,
    J1939_FMI_ABNORMAL_RATE_OF_CHANGE = 10,
    J1939_FMI_ROOT_CAUSE_NOT_KNOWN = 11,
    J1939_FMI_BAD_DEVICE = 12,
    J1939_FMI_OUT_OF_CALIBRATION = 13,
    J1939_FMI_SPECIAL_INSTRUCTIONS = 14,
    J1939_FMI_CONDITION_EXISTS = 31,
} lq_j1939_fmi_t;

/* J1939 29-bit Identifier Construction */
typedef struct {
    uint8_t priority;       /* 3 bits: 0 (highest) to 7 (lowest) */
    uint8_t edp;            /* Extended Data Page */
    uint8_t dp;             /* Data Page */
    uint8_t pf;             /* PDU Format */
    uint8_t ps;             /* PDU Specific */
    uint8_t sa;             /* Source Address */
} lq_j1939_id_t;

/* Build J1939 29-bit CAN identifier */
static inline uint32_t lq_j1939_build_id(const lq_j1939_id_t *id)
{
    return ((uint32_t)id->priority << 26) |
           ((uint32_t)id->edp << 25) |
           ((uint32_t)id->dp << 24) |
           ((uint32_t)id->pf << 16) |
           ((uint32_t)id->ps << 8) |
           ((uint32_t)id->sa << 0);
}

/* Build J1939 ID from PGN */
static inline uint32_t lq_j1939_build_id_from_pgn(uint32_t pgn, uint8_t priority, uint8_t sa)
{
    lq_j1939_id_t id = {
        .priority = priority,
        .edp = (uint8_t)((pgn >> 17) & 0x01U),
        .dp = (uint8_t)((pgn >> 16) & 0x01U),
        .pf = (uint8_t)((pgn >> 8) & 0xFFU),
        .ps = (uint8_t)(pgn & 0xFFU),
        .sa = sa
    };
    return lq_j1939_build_id(&id);
}

/* Extract PGN from J1939 29-bit ID */
static inline uint32_t lq_j1939_extract_pgn(uint32_t id)
{
    return (id >> 8) & 0x3FFFF;
}

/* Format DM1 message into 8-byte CAN frame */
int lq_j1939_format_dm1(const lq_j1939_dm1_t *dm1, uint8_t *data, size_t data_len);

/* Format DM0 (Broadcast of stop/warning lamps) */
int lq_j1939_format_dm0(lq_j1939_lamp_t stop_lamp, lq_j1939_lamp_t warning_lamp, 
                        uint8_t *data, size_t data_len);

/* Create DTC from SPN, FMI, and Occurrence Count */
static inline uint32_t lq_j1939_create_dtc(uint32_t spn, uint8_t fmi, uint8_t oc)
{
    return ((spn & 0x7FFFF) << 0) |     /* SPN: 19 bits */
           ((fmi & 0x1F) << 19) |        /* FMI: 5 bits */
           ((oc & 0x7F) << 24);          /* OC: 7 bits */
}

/* Extract components from DTC */
static inline uint32_t lq_j1939_get_spn(uint32_t dtc) { return dtc & 0x7FFFF; }
static inline uint8_t lq_j1939_get_fmi(uint32_t dtc) { return (dtc >> 19) & 0x1F; }
static inline uint8_t lq_j1939_get_oc(uint32_t dtc) { return (dtc >> 24) & 0x7F; }

/* Common J1939 Suspect Parameter Numbers (SPNs) */
#define J1939_SPN_ENGINE_SPEED          190
#define J1939_SPN_ENGINE_COOLANT_TEMP   110
#define J1939_SPN_ENGINE_OIL_PRESSURE   100
#define J1939_SPN_ENGINE_OIL_TEMP       175
#define J1939_SPN_FUEL_RATE             183
#define J1939_SPN_VEHICLE_SPEED         84
#define J1939_SPN_ACCEL_PEDAL_POS       91

/* J1939 Protocol Driver Context */
struct lq_j1939_ctx {
    uint8_t node_address;         /* Our J1939 address on the bus */
    
    /* Cached signal values for TX */
    struct {
        uint32_t signal_id;
        int32_t value;
        uint64_t timestamp;
    } signals[32];
    size_t num_signals;
    
    /* Cyclic message tracking */
    struct {
        uint32_t pgn;
        uint64_t last_tx_time;
        uint32_t period_ms;
    } cyclic_msgs[16];
    size_t num_cyclic;
    
    /* Diagnostic state */
    lq_j1939_dm1_t dm1;
};

/**
 * @brief Create J1939 protocol driver instance
 * 
 * @param proto Protocol driver to initialize
 * @param ctx J1939 context (caller-provided, no dynamic allocation)
 * @param config Protocol configuration (node address, mappings, etc)
 * @return 0 on success, negative errno on failure
 */
int lq_j1939_protocol_create(struct lq_protocol_driver *proto,
                              struct lq_j1939_ctx *ctx,
                              const struct lq_protocol_config *config);

/**
 * @brief J1939 Protocol Driver vtable
 * 
 * Exported for advanced users who want to customize behavior.
 */
extern const struct lq_protocol_vtbl lq_j1939_protocol_vtbl;

/* Legacy utility functions for direct message formatting */

/* Format DM1 message into 8-byte CAN frame */
int lq_j1939_format_dm1(const lq_j1939_dm1_t *dm1, uint8_t *data, size_t data_len);

/* Format DM0 (Broadcast of stop/warning lamps) */
int lq_j1939_format_dm0(lq_j1939_lamp_t stop_lamp, lq_j1939_lamp_t warning_lamp, 
                        uint8_t *data, size_t data_len);

/**
 * @brief Decode J1939 EEC1 message (Engine Controller 1)
 * 
 * @param data CAN message data (8 bytes)
 * @param rpm Output: Engine speed in RPM
 * @param torque Output: Engine torque (% of reference)
 * @return 0 on success, -1 on error
 */
int lq_j1939_decode_eec1(const uint8_t *data, uint16_t *rpm, uint8_t *torque);

/**
 * @brief Encode J1939 EEC1 message
 * 
 * @param rpm Engine speed in RPM
 * @param torque Engine torque (% of reference)
 * @param data Output buffer (must be 8 bytes)
 * @return 0 on success, -1 on error
 */
int lq_j1939_encode_eec1(uint16_t rpm, uint8_t torque, uint8_t *data);

/**
 * @brief Decode J1939 DM1 message
 * 
 * @param data CAN message data
 * @param len Data length
 * @param dm1 Output DM1 structure
 * @return 0 on success, -1 on error
 */
int lq_j1939_decode_dm1(const uint8_t *data, size_t len, lq_j1939_dm1_t *dm1);

#ifdef __cplusplus
}
#endif

#endif /* LQ_J1939_H_ */
