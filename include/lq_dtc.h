/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Diagnostic Trouble Code (DTC) Management
 * Supports J1939 DM1/DM2 and ISO 14229 (UDS) formats
 */

#ifndef LQ_DTC_H_
#define LQ_DTC_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum DTCs that can be stored */
#ifndef LQ_MAX_DTCS
#define LQ_MAX_DTCS 32
#endif

/* J1939 Failure Mode Identifier (FMI) codes */
enum lq_fmi {
    LQ_FMI_DATA_VALID_ABOVE_NORMAL = 0,
    LQ_FMI_DATA_VALID_BELOW_NORMAL = 1,
    LQ_FMI_DATA_ERRATIC = 2,
    LQ_FMI_VOLTAGE_ABOVE_NORMAL = 3,
    LQ_FMI_VOLTAGE_BELOW_NORMAL = 4,
    LQ_FMI_CURRENT_BELOW_NORMAL = 5,
    LQ_FMI_CURRENT_ABOVE_NORMAL = 6,
    LQ_FMI_MECHANICAL_FAILURE = 7,
    LQ_FMI_ABNORMAL_FREQUENCY = 8,
    LQ_FMI_ABNORMAL_UPDATE_RATE = 9,
    LQ_FMI_ABNORMAL_RATE_OF_CHANGE = 10,
    LQ_FMI_ROOT_CAUSE_NOT_KNOWN = 11,
    LQ_FMI_BAD_DEVICE = 12,
    LQ_FMI_OUT_OF_CALIBRATION = 13,
    LQ_FMI_SPECIAL_INSTRUCTIONS = 14,
    LQ_FMI_DATA_ABOVE_NORMAL_LEAST_SEVERE = 15,
    LQ_FMI_DATA_ABOVE_NORMAL_MODERATE = 16,
    LQ_FMI_DATA_BELOW_NORMAL_LEAST_SEVERE = 17,
    LQ_FMI_DATA_BELOW_NORMAL_MODERATE = 18,
    LQ_FMI_RECEIVED_NETWORK_DATA_IN_ERROR = 19,
    LQ_FMI_DATA_DRIFTED_HIGH = 20,
    LQ_FMI_DATA_DRIFTED_LOW = 21,
    LQ_FMI_NOT_AVAILABLE = 31,
};

/* Malfunction Indicator Lamp status (J1939 DM1)
 * Values ordered by severity - higher = more severe */
enum lq_lamp_status {
    LQ_LAMP_OFF = 0,
    LQ_LAMP_AMBER = 1,          /* Amber/Yellow warning */
    LQ_LAMP_AMBER_FLASH = 2,    /* Flashing amber */
    LQ_LAMP_RED = 3,            /* Red stop lamp - highest severity */
};

/* DTC lifecycle states */
enum lq_dtc_state {
    LQ_DTC_INACTIVE = 0,        /* Not present */
    LQ_DTC_PENDING = 1,         /* Fault detected but not confirmed */
    LQ_DTC_CONFIRMED = 2,       /* Fault confirmed active */
    LQ_DTC_STORED = 3,          /* Fault cleared but stored in memory */
};

/* DTC entry */
struct lq_dtc {
    uint32_t spn;               /* Suspect Parameter Number (J1939) */
    uint8_t fmi;                /* Failure Mode Identifier */
    uint8_t occurrence_count;   /* How many times fault occurred */
    enum lq_dtc_state state;
    enum lq_lamp_status lamp;   /* MIL/warning lamp status */
    uint64_t first_detected_us; /* Timestamp of first detection */
    uint64_t last_active_us;    /* Timestamp of last active occurrence */
};

/* DTC manager state */
struct lq_dtc_manager {
    struct lq_dtc dtcs[LQ_MAX_DTCS];
    uint8_t num_active;         /* Count of active/confirmed DTCs */
    uint8_t num_stored;         /* Count of stored (historical) DTCs */
    uint64_t last_dm1_send_us;  /* Rate limiting for DM1 broadcast */
    uint32_t dm1_period_ms;     /* DM1 broadcast period (default 1000ms) */
};

/* ============================================================================
 * DTC Manager API
 * ============================================================================ */

/**
 * Initialize DTC manager
 * @param mgr DTC manager instance
 * @param dm1_period_ms Period for DM1 broadcasts (typically 1000ms)
 */
void lq_dtc_init(struct lq_dtc_manager *mgr, uint32_t dm1_period_ms);

/**
 * Set a DTC to active state
 * @param mgr DTC manager
 * @param spn Suspect Parameter Number
 * @param fmi Failure Mode Identifier
 * @param lamp Malfunction lamp status
 * @param now Current timestamp in microseconds
 * @return 0 on success, -ENOMEM if DTC table full
 */
int lq_dtc_set_active(struct lq_dtc_manager *mgr, uint32_t spn, uint8_t fmi,
                      enum lq_lamp_status lamp, uint64_t now);

/**
 * Clear a DTC (move to stored state if configured, or remove)
 * @param mgr DTC manager
 * @param spn Suspect Parameter Number
 * @param fmi Failure Mode Identifier
 * @param now Current timestamp
 * @return 0 on success, -ENOENT if DTC not found
 */
int lq_dtc_clear(struct lq_dtc_manager *mgr, uint32_t spn, uint8_t fmi, uint64_t now);

/**
 * Clear all DTCs
 * @param mgr DTC manager
 */
void lq_dtc_clear_all(struct lq_dtc_manager *mgr);

/**
 * Get count of active DTCs
 */
uint8_t lq_dtc_get_active_count(const struct lq_dtc_manager *mgr);

/**
 * Get count of stored (historical) DTCs
 */
uint8_t lq_dtc_get_stored_count(const struct lq_dtc_manager *mgr);

/**
 * Get highest severity lamp status among all active DTCs
 * @return Highest severity lamp (RED > AMBER_FLASH > AMBER > OFF)
 */
enum lq_lamp_status lq_dtc_get_mil_status(const struct lq_dtc_manager *mgr);

/* ============================================================================
 * J1939 DM1 (Active DTCs) Message Generation
 * ============================================================================ */

/**
 * Build J1939 DM1 message (PGN 65226 / 0xFECA)
 * Format: [lamp_status, flash_status, ...DTCs...]
 * Each DTC: 4 bytes [SPN_low, SPN_mid, SPN_high+FMI, occurrence_count]
 * 
 * @param mgr DTC manager
 * @param data Output buffer (minimum 8 bytes)
 * @param max_size Maximum buffer size
 * @param now Current timestamp
 * @return Number of bytes written, or 0 if not time to send yet
 */
int lq_dtc_build_dm1(struct lq_dtc_manager *mgr, uint8_t *data, 
                     size_t max_size, uint64_t now);

/**
 * Build J1939 DM2 message (Previously Active DTCs)
 * Same format as DM1 but for stored/cleared faults
 * PGN 65227 / 0xFECB
 */
int lq_dtc_build_dm2(struct lq_dtc_manager *mgr, uint8_t *data, size_t max_size);

#ifdef __cplusplus
}
#endif

#endif /* LQ_DTC_H_ */
