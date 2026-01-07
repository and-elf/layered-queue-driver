/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDS (Unified Diagnostic Services) - ISO 14229
 * 
 * Automotive diagnostic and calibration protocol with multi-transport support.
 * Enables secure configuration, parameter access, and routine control.
 * 
 * Supported Services:
 * - 0x10: Diagnostic Session Control (Default, Programming, Extended)
 * - 0x27: Security Access (Seed/Key authentication)
 * - 0x22: Read Data By Identifier
 * - 0x2E: Write Data By Identifier
 * - 0x31: Routine Control (Start/Stop/Request Results)
 * - 0x3E: Tester Present (keep session alive)
 * 
 * Transport Abstraction:
 * - ISO-TP over CAN (ISO 15765-2)
 * - DoIP over Ethernet (ISO 13400)
 * - UDS on LIN, FlexRay, etc.
 */

#ifndef LQ_UDS_H_
#define LQ_UDS_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * UDS Service Identifiers (SID)
 * ============================================================================ */

#define UDS_SID_DIAGNOSTIC_SESSION_CONTROL      0x10
#define UDS_SID_ECU_RESET                       0x11
#define UDS_SID_SECURITY_ACCESS                 0x27
#define UDS_SID_COMMUNICATION_CONTROL           0x28
#define UDS_SID_TESTER_PRESENT                  0x3E
#define UDS_SID_READ_DATA_BY_IDENTIFIER         0x22
#define UDS_SID_READ_MEMORY_BY_ADDRESS          0x23
#define UDS_SID_READ_SCALING_DATA_BY_IDENTIFIER 0x24
#define UDS_SID_WRITE_DATA_BY_IDENTIFIER        0x2E
#define UDS_SID_WRITE_MEMORY_BY_ADDRESS         0x3D
#define UDS_SID_ROUTINE_CONTROL                 0x31
#define UDS_SID_REQUEST_DOWNLOAD                0x34
#define UDS_SID_REQUEST_UPLOAD                  0x35
#define UDS_SID_TRANSFER_DATA                   0x36
#define UDS_SID_REQUEST_TRANSFER_EXIT           0x37

/* Positive response: SID + 0x40 */
#define UDS_POSITIVE_RESPONSE_OFFSET            0x40

/* Negative response SID */
#define UDS_SID_NEGATIVE_RESPONSE               0x7F

/* ============================================================================
 * Negative Response Codes (NRC)
 * ============================================================================ */

#define UDS_NRC_POSITIVE_RESPONSE                    0x00
#define UDS_NRC_GENERAL_REJECT                       0x10
#define UDS_NRC_SERVICE_NOT_SUPPORTED                0x11
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED            0x12
#define UDS_NRC_INCORRECT_MESSAGE_LENGTH             0x13
#define UDS_NRC_RESPONSE_TOO_LONG                    0x14
#define UDS_NRC_BUSY_REPEAT_REQUEST                  0x21
#define UDS_NRC_CONDITIONS_NOT_CORRECT               0x22
#define UDS_NRC_REQUEST_SEQUENCE_ERROR               0x24
#define UDS_NRC_REQUEST_OUT_OF_RANGE                 0x31
#define UDS_NRC_SECURITY_ACCESS_DENIED               0x33
#define UDS_NRC_INVALID_KEY                          0x35
#define UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS            0x36
#define UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED      0x37
#define UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED         0x70
#define UDS_NRC_TRANSFER_DATA_SUSPENDED              0x71
#define UDS_NRC_GENERAL_PROGRAMMING_FAILURE          0x72
#define UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER         0x73
#define UDS_NRC_RESPONSE_PENDING                     0x78
#define UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION 0x7E
#define UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION     0x7F

/* Negative NRC values for internal error handling (avoid -errno conflicts) */
#define LQ_NRC_GENERAL_REJECT                       (-UDS_NRC_GENERAL_REJECT)
#define LQ_NRC_SERVICE_NOT_SUPPORTED                (-UDS_NRC_SERVICE_NOT_SUPPORTED)
#define LQ_NRC_SUBFUNCTION_NOT_SUPPORTED            (-UDS_NRC_SUBFUNCTION_NOT_SUPPORTED)
#define LQ_NRC_INCORRECT_MESSAGE_LENGTH             (-UDS_NRC_INCORRECT_MESSAGE_LENGTH)
#define LQ_NRC_RESPONSE_TOO_LONG                    (-UDS_NRC_RESPONSE_TOO_LONG)
#define LQ_NRC_BUSY_REPEAT_REQUEST                  (-UDS_NRC_BUSY_REPEAT_REQUEST)
#define LQ_NRC_CONDITIONS_NOT_CORRECT               (-UDS_NRC_CONDITIONS_NOT_CORRECT)
#define LQ_NRC_REQUEST_SEQUENCE_ERROR               (-UDS_NRC_REQUEST_SEQUENCE_ERROR)
#define LQ_NRC_REQUEST_OUT_OF_RANGE                 (-UDS_NRC_REQUEST_OUT_OF_RANGE)
#define LQ_NRC_SECURITY_ACCESS_DENIED               (-UDS_NRC_SECURITY_ACCESS_DENIED)
#define LQ_NRC_INVALID_KEY                          (-UDS_NRC_INVALID_KEY)
#define LQ_NRC_EXCEED_NUMBER_OF_ATTEMPTS            (-UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS)
#define LQ_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED      (-UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED)
#define LQ_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED         (-UDS_NRC_UPLOAD_DOWNLOAD_NOT_ACCEPTED)
#define LQ_NRC_TRANSFER_DATA_SUSPENDED              (-UDS_NRC_TRANSFER_DATA_SUSPENDED)
#define LQ_NRC_GENERAL_PROGRAMMING_FAILURE          (-UDS_NRC_GENERAL_PROGRAMMING_FAILURE)
#define LQ_NRC_WRONG_BLOCK_SEQUENCE_COUNTER         (-UDS_NRC_WRONG_BLOCK_SEQUENCE_COUNTER)
#define LQ_NRC_RESPONSE_PENDING                     (-UDS_NRC_RESPONSE_PENDING)
#define LQ_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION (-UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_SESSION)
#define LQ_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION     (-UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION)

/* ============================================================================
 * Diagnostic Session Types
 * ============================================================================ */

enum lq_uds_session {
    UDS_SESSION_DEFAULT                 = 0x01,  /* Normal operation */
    UDS_SESSION_PROGRAMMING             = 0x02,  /* Flash programming */
    UDS_SESSION_EXTENDED_DIAGNOSTIC     = 0x03,  /* Extended diagnostics */
    UDS_SESSION_SAFETY_SYSTEM           = 0x04,  /* Safety system diagnostic */
};

/* ============================================================================
 * Security Access Levels
 * ============================================================================ */

enum lq_uds_security_level {
    UDS_SECURITY_LOCKED                 = 0x00,  /* No security access */
    UDS_SECURITY_LEVEL_1                = 0x01,  /* Basic access */
    UDS_SECURITY_LEVEL_2                = 0x02,  /* Extended access */
    UDS_SECURITY_LEVEL_3                = 0x03,  /* Programming access */
};

/* ============================================================================
 * Routine Control Sub-functions
 * ============================================================================ */

enum lq_uds_routine_control_type {
    LQ_UDS_ROUTINE_START                = 0x01,
    LQ_UDS_ROUTINE_STOP                 = 0x02,
    LQ_UDS_ROUTINE_REQUEST_RESULTS      = 0x03,
    
    /* Aliases */
    UDS_ROUTINE_START                   = 0x01,
    UDS_ROUTINE_STOP                    = 0x02,
    UDS_ROUTINE_REQUEST_RESULTS         = 0x03,
};

/* ============================================================================
 * Data Identifiers (DIDs) - Application Specific
 * ============================================================================ */

/* Standard DIDs */
#define UDS_DID_ACTIVE_DIAGNOSTIC_SESSION       0xF186
#define UDS_DID_VIN                             0xF190
#define UDS_DID_ECU_SERIAL_NUMBER               0xF18C
#define UDS_DID_ECU_HARDWARE_VERSION            0xF191
#define UDS_DID_ECU_SOFTWARE_VERSION            0xF192
#define UDS_DID_PROGRAMMING_DATE                0xF199

/* Custom DIDs for layered queue configuration (0xF1A0-0xF1FF reserved) */
#define LQ_DID_SIGNAL_VALUE                     0xF1A0  /* Read signal value */
#define LQ_DID_SIGNAL_STATUS                    0xF1A1  /* Read signal status */
#define LQ_DID_REMAP_CONFIG                     0xF1A2  /* Read/Write remap config */
#define LQ_DID_SCALE_CONFIG                     0xF1A3  /* Read/Write scale config */
#define LQ_DID_FAULT_STATUS                     0xF1A4  /* Read fault status */
#define LQ_DID_DTC_STATUS                       0xF1A5  /* Read DTC status */
#define LQ_DID_CALIBRATION_MODE                 0xF1A6  /* Read calibration mode status */

/* Aliases for backward compatibility */
#define UDS_DID_LQ_SIGNAL_VALUE                 LQ_DID_SIGNAL_VALUE
#define UDS_DID_LQ_SIGNAL_STATUS                LQ_DID_SIGNAL_STATUS
#define UDS_DID_LQ_REMAP_CONFIG                 LQ_DID_REMAP_CONFIG
#define UDS_DID_LQ_SCALE_CONFIG                 LQ_DID_SCALE_CONFIG
#define UDS_DID_LQ_FAULT_STATUS                 LQ_DID_FAULT_STATUS
#define UDS_DID_LQ_DTC_STATUS                   LQ_DID_DTC_STATUS
#define UDS_DID_LQ_CALIBRATION_MODE             LQ_DID_CALIBRATION_MODE

/* ============================================================================
 * Routine Identifiers (RIDs) - Application Specific
 * ============================================================================ */

#define UDS_RID_ERASE_MEMORY                    0xFF00
#define UDS_RID_CHECK_PROGRAMMING_DEPENDENCIES  0xFF01
#define LQ_RID_ENTER_CALIBRATION                0xF1A0  /* Enter safe config mode */
#define LQ_RID_EXIT_CALIBRATION                 0xF1A1  /* Exit and validate config */
#define LQ_RID_RESET_DEFAULTS                   0xF1A2  /* Reset all config to defaults */

/* Aliases for backward compatibility */
#define UDS_RID_ENTER_CALIBRATION_MODE          LQ_RID_ENTER_CALIBRATION
#define UDS_RID_EXIT_CALIBRATION_MODE           LQ_RID_EXIT_CALIBRATION
#define UDS_RID_RESET_TO_DEFAULTS               LQ_RID_RESET_DEFAULTS

/* ============================================================================
 * Transport Layer Abstraction
 * ============================================================================ */

/**
 * @brief UDS transport layer interface
 * 
 * Abstraction for different physical layers (CAN, Ethernet, Serial, etc.)
 */
struct lq_uds_transport_ops {
    /**
     * @brief Send UDS response
     * 
     * @param ctx Transport context
     * @param data Response data
     * @param len Response length
     * @return 0 on success, negative errno on failure
     */
    int (*send)(void *ctx, const uint8_t *data, size_t len);
    
    /**
     * @brief Get maximum payload size for this transport
     * 
     * @param ctx Transport context
     * @return Maximum payload size in bytes
     */
    size_t (*get_max_payload)(void *ctx);
};

/**
 * @brief UDS transport instance
 */
struct lq_uds_transport {
    const struct lq_uds_transport_ops *ops;
    void *ctx;
};

/* ============================================================================
 * Security Access Callbacks
 * ============================================================================ */

/**
 * @brief Security seed generator callback
 * 
 * @param level Security level being requested
 * @param seed Output buffer for seed (4 bytes)
 * @return 0 on success, negative errno on failure
 */
typedef int (*lq_uds_get_seed_fn)(uint8_t level, uint8_t *seed);

/**
 * @brief Security key validator callback
 * 
 * @param level Security level being unlocked
 * @param key Key from tester (4 bytes)
 * @return true if key is valid, false otherwise
 */
typedef bool (*lq_uds_verify_key_fn)(uint8_t level, const uint8_t *key);

/* ============================================================================
 * Data Identifier Callbacks
 * ============================================================================ */

/**
 * @brief Read DID callback
 * 
 * @param did Data identifier
 * @param data Output buffer for data
 * @param max_len Maximum data length
 * @param actual_len Output: actual data length written
 * @return 0 on success, negative UDS_NRC on error
 */
typedef int (*lq_uds_read_did_fn)(uint16_t did, uint8_t *data, size_t max_len, size_t *actual_len);

/**
 * @brief Write DID callback
 * 
 * @param did Data identifier
 * @param data Data to write
 * @param len Data length
 * @return 0 on success, negative UDS_NRC on error
 */
typedef int (*lq_uds_write_did_fn)(uint16_t did, const uint8_t *data, size_t len);

/* ============================================================================
 * Routine Control Callbacks
 * ============================================================================ */

/**
 * @brief Routine control callback
 * 
 * @param rid Routine identifier
 * @param control_type Start/Stop/Request Results
 * @param in_data Input parameters
 * @param in_len Input length
 * @param out_data Output buffer for results
 * @param max_out Maximum output length
 * @param actual_out Output: actual result length
 * @return 0 on success, negative UDS_NRC on error
 */
typedef int (*lq_uds_routine_fn)(uint16_t rid, uint8_t control_type,
                                  const uint8_t *in_data, size_t in_len,
                                  uint8_t *out_data, size_t max_out, size_t *actual_out);

/* ============================================================================
 * UDS Server Configuration
 * ============================================================================ */

/**
 * @brief UDS server configuration
 */
struct lq_uds_config {
    /* Transport layer */
    struct lq_uds_transport *transport;
    
    /* Security callbacks */
    lq_uds_get_seed_fn get_seed;
    lq_uds_verify_key_fn verify_key;
    
    /* Data identifier callbacks */
    lq_uds_read_did_fn read_did;
    lq_uds_write_did_fn write_did;
    
    /* Routine control callback */
    lq_uds_routine_fn routine_control;
    
    /* Timing parameters (milliseconds) */
    uint32_t p2_server_max;           /* Max time for response (default: 50ms) */
    uint32_t p2_star_server_max;      /* Max time for pending response (default: 5000ms) */
    uint32_t s3_server;               /* Session timeout (default: 5000ms) */
    
    /* Security parameters */
    uint32_t security_delay_ms;       /* Delay after failed attempt (default: 10000ms) */
    uint8_t max_security_attempts;    /* Max attempts before lockout (default: 3) */
};

/**
 * @brief UDS server state
 */
struct lq_uds_server {
    struct lq_uds_config config;
    
    /* Session state */
    enum lq_uds_session current_session;
    uint64_t session_start_time;      /* For S3 timeout */
    uint64_t last_activity_time;
    
    /* Security state */
    enum lq_uds_security_level security_level;
    uint8_t security_attempts;
    uint64_t security_lockout_until;  /* Timestamp when lockout expires */
    uint8_t current_seed[4];
    
    /* Pending response handling */
    bool response_pending;
    uint8_t pending_sid;
};

/* ============================================================================
 * UDS Server API
 * ============================================================================ */

/**
 * @brief Initialize UDS server
 * 
 * @param server UDS server instance
 * @param config Configuration
 * @return 0 on success, negative errno on failure
 */
int lq_uds_server_init(struct lq_uds_server *server, const struct lq_uds_config *config);

/**
 * @brief Process incoming UDS request
 * 
 * This is the main entry point for processing UDS requests.
 * It handles the request, updates state, and sends the response.
 * 
 * @param server UDS server instance
 * @param request Request data
 * @param len Request length
 * @param now Current timestamp (microseconds)
 * @return 0 on success, negative errno on failure
 */
int lq_uds_server_process_request(struct lq_uds_server *server,
                                   const uint8_t *request, size_t len,
                                   uint64_t now);

/**
 * @brief Process periodic tasks (session timeout, etc.)
 * 
 * Call this periodically (e.g., every 100ms) to handle timeouts.
 * 
 * @param server UDS server instance
 * @param now Current timestamp (microseconds)
 */
void lq_uds_server_periodic(struct lq_uds_server *server, uint64_t now);

/**
 * @brief Get current diagnostic session
 * 
 * @param server UDS server instance
 * @return Current session type
 */
enum lq_uds_session lq_uds_get_session(const struct lq_uds_server *server);

/**
 * @brief Get current security level
 * 
 * @param server UDS server instance
 * @return Current security level
 */
enum lq_uds_security_level lq_uds_get_security_level(const struct lq_uds_server *server);

/**
 * @brief Check if a service is allowed in current session/security state
 * 
 * @param server UDS server instance
 * @param sid Service identifier
 * @return true if allowed, false otherwise
 */
bool lq_uds_is_service_allowed(const struct lq_uds_server *server, uint8_t sid);

/* ============================================================================
 * Helper Functions for Building Responses
 * ============================================================================ */

/**
 * @brief Build positive response
 * 
 * @param response Output buffer
 * @param sid Service identifier
 * @param data Response data (optional)
 * @param data_len Response data length
 * @return Total response length
 */
size_t lq_uds_build_positive_response(uint8_t *response, uint8_t sid,
                                       const uint8_t *data, size_t data_len);

/**
 * @brief Build negative response
 * 
 * @param response Output buffer
 * @param sid Service identifier that failed
 * @param nrc Negative response code
 * @return Total response length (always 3)
 */
size_t lq_uds_build_negative_response(uint8_t *response, uint8_t sid, uint8_t nrc);

#ifdef __cplusplus
}
#endif

#endif /* LQ_UDS_H_ */
