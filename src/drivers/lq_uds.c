/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDS (Unified Diagnostic Services) - ISO 14229 Implementation
 */

#include "lq_uds.h"
#include "lq_platform.h"
#include <string.h>
#include <errno.h>

/* Maximum UDS message size */
#define UDS_MAX_MESSAGE_SIZE    4095

/* Helper macro for logging */
#ifndef LQ_UDS_LOG
#define LQ_UDS_LOG(fmt, ...) LQ_LOG_INF("UDS", fmt, ##__VA_ARGS__)
#endif

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Send response through transport layer
 */
static int send_response(struct lq_uds_server *server, const uint8_t *data, size_t len)
{
    if (!server->config.transport || !server->config.transport->ops->send) {
        return -ENOTSUP;
    }
    
    return server->config.transport->ops->send(server->config.transport->ctx, data, len);
}

/**
 * @brief Send negative response
 */
static int send_negative_response(struct lq_uds_server *server, uint8_t sid, uint8_t nrc)
{
    uint8_t response[3];
    size_t len = lq_uds_build_negative_response(response, sid, nrc);
    LQ_UDS_LOG("Negative response: SID=0x%02X NRC=0x%02X", sid, nrc);
    return send_response(server, response, len);
}

/**
 * @brief Check if security level allows access
 */
static bool check_security_access(const struct lq_uds_server *server, 
                                   enum lq_uds_security_level required_level)
{
    return server->security_level >= required_level;
}

/**
 * @brief Check if current session allows service
 */
static bool check_session_for_service(const struct lq_uds_server *server, uint8_t sid)
{
    /* Tester Present and Session Control always allowed */
    if (sid == UDS_SID_TESTER_PRESENT || sid == UDS_SID_DIAGNOSTIC_SESSION_CONTROL) {
        return true;
    }
    
    /* Security Access allowed in all non-default sessions */
    if (sid == UDS_SID_SECURITY_ACCESS && 
        server->current_session != UDS_SESSION_DEFAULT) {
        return true;
    }
    
    /* Read services allowed in all sessions */
    if (sid == UDS_SID_READ_DATA_BY_IDENTIFIER) {
        return true;
    }
    
    /* Write and Routine services require extended or programming session */
    if (sid == UDS_SID_WRITE_DATA_BY_IDENTIFIER || 
        sid == UDS_SID_ROUTINE_CONTROL) {
        return (server->current_session == UDS_SESSION_EXTENDED_DIAGNOSTIC ||
                server->current_session == UDS_SESSION_PROGRAMMING);
    }
    
    /* Programming services require programming session */
    if (sid == UDS_SID_REQUEST_DOWNLOAD || 
        sid == UDS_SID_REQUEST_UPLOAD ||
        sid == UDS_SID_TRANSFER_DATA ||
        sid == UDS_SID_REQUEST_TRANSFER_EXIT) {
        return server->current_session == UDS_SESSION_PROGRAMMING;
    }
    
    /* Default: allow in extended and programming sessions */
    return (server->current_session != UDS_SESSION_DEFAULT);
}

/* ============================================================================
 * Service Handlers
 * ============================================================================ */

/**
 * @brief Handle Diagnostic Session Control (0x10)
 */
static int handle_diagnostic_session_control(struct lq_uds_server *server,
                                               const uint8_t *request, size_t len,
                                               uint64_t now)
{
    if (len < 2) {
        return send_negative_response(server, UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    uint8_t session_type = request[1];
    
    /* Validate session type */
    if (session_type < UDS_SESSION_DEFAULT || session_type > UDS_SESSION_SAFETY_SYSTEM) {
        return send_negative_response(server, UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                       UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    
    /* Switch session */
    server->current_session = (enum lq_uds_session)session_type;
    server->session_start_time = now;
    server->last_activity_time = now;
    
    /* Reset security on session change (except default->default) */
    if (session_type != UDS_SESSION_DEFAULT) {
        server->security_level = UDS_SECURITY_LOCKED;
    }
    
    /* Build positive response */
    uint8_t response[6];
    response[0] = UDS_SID_DIAGNOSTIC_SESSION_CONTROL + UDS_POSITIVE_RESPONSE_OFFSET;
    response[1] = session_type;
    
    /* P2 timing in 10ms units */
    uint16_t p2 = (uint16_t)(server->config.p2_server_max / 10U);
    response[2] = (uint8_t)((p2 >> 8) & 0xFFU);
    response[3] = (uint8_t)(p2 & 0xFFU);
    
    /* P2* timing in 10ms units */
    uint16_t p2_star = (uint16_t)(server->config.p2_star_server_max / 10U);
    response[4] = (uint8_t)((p2_star >> 8) & 0xFFU);
    response[5] = (uint8_t)(p2_star & 0xFFU);
    
    LQ_UDS_LOG("Session changed to 0x%02X", session_type);
    return send_response(server, response, 6);
}

/**
 * @brief Handle Security Access (0x27)
 */
static int handle_security_access(struct lq_uds_server *server,
                                    const uint8_t *request, size_t len,
                                    uint64_t now)
{
    if (len < 2) {
        return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    uint8_t sub_function = request[1];
    bool is_seed_request = (sub_function & 0x01) != 0;
    uint8_t level = (uint8_t)((sub_function + 1U) / 2U);  /* Convert to level (1, 2, 3...) */
    
    /* Check if in security lockout */
    if (now < server->security_lockout_until) {
        return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                       UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED);
    }
    
    if (is_seed_request) {
        /* Request Seed */
        if (len != 2) {
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_INCORRECT_MESSAGE_LENGTH);
        }
        
        /* Check if already unlocked at this level or higher */
        if (server->security_level >= level) {
            /* Already unlocked - return zero seed */
            uint8_t response[6] = {
                UDS_SID_SECURITY_ACCESS + UDS_POSITIVE_RESPONSE_OFFSET,
                sub_function,
                0, 0, 0, 0  /* Zero seed */
            };
            return send_response(server, response, 6);
        }
        
        /* Generate seed */
        if (!server->config.get_seed) {
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_CONDITIONS_NOT_CORRECT);
        }
        
        if (server->config.get_seed(level, server->current_seed) != 0) {
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_CONDITIONS_NOT_CORRECT);
        }
        
        /* Send seed */
        uint8_t response[6] = {
            UDS_SID_SECURITY_ACCESS + UDS_POSITIVE_RESPONSE_OFFSET,
            sub_function,
            server->current_seed[0],
            server->current_seed[1],
            server->current_seed[2],
            server->current_seed[3]
        };
        
        LQ_UDS_LOG("Seed generated for level %d", level);
        return send_response(server, response, 6);
        
    } else {
        /* Send Key */
        if (len != 6) {
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_INCORRECT_MESSAGE_LENGTH);
        }
        
        /* Verify key */
        if (!server->config.verify_key) {
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_CONDITIONS_NOT_CORRECT);
        }
        
        bool valid = server->config.verify_key(level, &request[2]);
        
        if (!valid) {
            server->security_attempts++;
            
            if (server->security_attempts >= server->config.max_security_attempts) {
                /* Lockout */
                server->security_lockout_until = now + 
                    (server->config.security_delay_ms * 1000ULL);
                LQ_UDS_LOG("Security lockout - too many attempts");
                return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                               UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS);
            }
            
            LQ_UDS_LOG("Invalid key - attempt %d/%d", 
                      server->security_attempts,
                      server->config.max_security_attempts);
            return send_negative_response(server, UDS_SID_SECURITY_ACCESS,
                                           UDS_NRC_INVALID_KEY);
        }
        
        /* Key is valid - unlock */
        server->security_level = (enum lq_uds_security_level)level;
        server->security_attempts = 0;
        
        uint8_t response[2] = {
            UDS_SID_SECURITY_ACCESS + UDS_POSITIVE_RESPONSE_OFFSET,
            sub_function
        };
        
        LQ_UDS_LOG("Security unlocked at level %d", level);
        return send_response(server, response, 2);
    }
}

/**
 * @brief Handle Tester Present (0x3E)
 */
static int handle_tester_present(struct lq_uds_server *server,
                                   const uint8_t *request, size_t len,
                                   uint64_t now)
{
    if (len < 2) {
        return send_negative_response(server, UDS_SID_TESTER_PRESENT,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    uint8_t sub_function = request[1] & 0x7F;  /* Suppress positive response bit in bit 7 */
    bool suppress_response = (request[1] & 0x80) != 0;
    
    if (sub_function != 0x00) {
        return send_negative_response(server, UDS_SID_TESTER_PRESENT,
                                       UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    
    /* Update activity time */
    server->last_activity_time = now;
    
    if (suppress_response) {
        return 0;  /* Don't send response */
    }
    
    uint8_t response[2] = {
        UDS_SID_TESTER_PRESENT + UDS_POSITIVE_RESPONSE_OFFSET,
        sub_function
    };
    
    return send_response(server, response, 2);
}

/**
 * @brief Handle Read Data By Identifier (0x22)
 */
static int handle_read_data_by_identifier(struct lq_uds_server *server,
                                            const uint8_t *request, size_t len)
{
    if (len < 3) {
        return send_negative_response(server, UDS_SID_READ_DATA_BY_IDENTIFIER,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    if (!server->config.read_did) {
        return send_negative_response(server, UDS_SID_READ_DATA_BY_IDENTIFIER,
                                       UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
    
    /* Extract DID (16-bit, big-endian) */
    uint16_t did = ((uint16_t)request[1] << 8) | request[2];
    
    /* Build response header */
    uint8_t response[UDS_MAX_MESSAGE_SIZE];
    response[0] = UDS_SID_READ_DATA_BY_IDENTIFIER + UDS_POSITIVE_RESPONSE_OFFSET;
    response[1] = request[1];  /* DID high */
    response[2] = request[2];  /* DID low */
    
    /* Read data */
    size_t data_len;
    int result = server->config.read_did(did, &response[3], 
                                          UDS_MAX_MESSAGE_SIZE - 3, &data_len);
    
    if (result < 0) {
        /* Callback returned NRC */
        return send_negative_response(server, UDS_SID_READ_DATA_BY_IDENTIFIER, (uint8_t)(-result));
    }
    
    LQ_UDS_LOG("Read DID 0x%04X: %zu bytes", did, data_len);
    return send_response(server, response, 3 + data_len);
}

/**
 * @brief Handle Write Data By Identifier (0x2E)
 */
static int handle_write_data_by_identifier(struct lq_uds_server *server,
                                             const uint8_t *request, size_t len)
{
    if (len < 4) {
        return send_negative_response(server, UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    if (!server->config.write_did) {
        return send_negative_response(server, UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                                       UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
    
    /* Check security access */
    if (!check_security_access(server, UDS_SECURITY_LEVEL_1)) {
        return send_negative_response(server, UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                                       UDS_NRC_SECURITY_ACCESS_DENIED);
    }
    
    /* Extract DID */
    uint16_t did = ((uint16_t)request[1] << 8) | request[2];
    
    /* Write data */
    int result = server->config.write_did(did, &request[3], len - 3);
    
    if (result < 0) {
        return send_negative_response(server, UDS_SID_WRITE_DATA_BY_IDENTIFIER, (uint8_t)(-result));
    }
    
    /* Positive response echoes DID */
    uint8_t response[3] = {
        UDS_SID_WRITE_DATA_BY_IDENTIFIER + UDS_POSITIVE_RESPONSE_OFFSET,
        request[1],
        request[2]
    };
    
    LQ_UDS_LOG("Write DID 0x%04X: %zu bytes", did, len - 3);
    return send_response(server, response, 3);
}

/**
 * @brief Handle Routine Control (0x31)
 */
static int handle_routine_control(struct lq_uds_server *server,
                                    const uint8_t *request, size_t len)
{
    if (len < 4) {
        return send_negative_response(server, UDS_SID_ROUTINE_CONTROL,
                                       UDS_NRC_INCORRECT_MESSAGE_LENGTH);
    }
    
    if (!server->config.routine_control) {
        return send_negative_response(server, UDS_SID_ROUTINE_CONTROL,
                                       UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
    
    /* Check security access for routine control */
    if (!check_security_access(server, UDS_SECURITY_LEVEL_1)) {
        return send_negative_response(server, UDS_SID_ROUTINE_CONTROL,
                                       UDS_NRC_SECURITY_ACCESS_DENIED);
    }
    
    uint8_t control_type = request[1];
    uint16_t rid = ((uint16_t)request[2] << 8) | request[3];
    
    /* Validate control type */
    if (control_type < UDS_ROUTINE_START || control_type > UDS_ROUTINE_REQUEST_RESULTS) {
        return send_negative_response(server, UDS_SID_ROUTINE_CONTROL,
                                       UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    
    /* Build response header */
    uint8_t response[UDS_MAX_MESSAGE_SIZE];
    response[0] = UDS_SID_ROUTINE_CONTROL + UDS_POSITIVE_RESPONSE_OFFSET;
    response[1] = control_type;
    response[2] = request[2];  /* RID high */
    response[3] = request[3];  /* RID low */
    
    /* Execute routine */
    size_t result_len;
    int result = server->config.routine_control(rid, control_type,
                                                 len > 4 ? &request[4] : NULL,
                                                 len > 4 ? len - 4 : 0,
                                                 &response[4],
                                                 UDS_MAX_MESSAGE_SIZE - 4,
                                                 &result_len);
    
    if (result < 0) {
        return send_negative_response(server, UDS_SID_ROUTINE_CONTROL, (uint8_t)(-result));
    }
    
    LQ_UDS_LOG("Routine 0x%04X control type %d: %zu bytes result", 
              rid, control_type, result_len);
    return send_response(server, response, 4 + result_len);
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int lq_uds_server_init(struct lq_uds_server *server, const struct lq_uds_config *config)
{
    if (!server || !config) {
        return -EINVAL;
    }
    
    memset(server, 0, sizeof(*server));
    memcpy(&server->config, config, sizeof(*config));
    
    /* Set default timing if not configured */
    if (server->config.p2_server_max == 0) {
        server->config.p2_server_max = 50;  /* 50ms */
    }
    if (server->config.p2_star_server_max == 0) {
        server->config.p2_star_server_max = 5000;  /* 5s */
    }
    if (server->config.s3_server == 0) {
        server->config.s3_server = 5000;  /* 5s */
    }
    if (server->config.security_delay_ms == 0) {
        server->config.security_delay_ms = 10000;  /* 10s */
    }
    if (server->config.max_security_attempts == 0) {
        server->config.max_security_attempts = 3;
    }
    
    /* Initialize state */
    server->current_session = UDS_SESSION_DEFAULT;
    server->security_level = UDS_SECURITY_LOCKED;
    
    LQ_UDS_LOG("UDS server initialized");
    return 0;
}

int lq_uds_server_process_request(struct lq_uds_server *server,
                                   const uint8_t *request, size_t len,
                                   uint64_t now)
{
    if (!server || !request || len == 0) {
        return -EINVAL;
    }
    
    uint8_t sid = request[0];
    
    /* Update activity time */
    server->last_activity_time = now;
    
    /* Check if service is supported */
    if (!check_session_for_service(server, sid)) {
        return send_negative_response(server, sid, 
                                       UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION);
    }
    
    /* Dispatch to service handler */
    switch (sid) {
        case UDS_SID_DIAGNOSTIC_SESSION_CONTROL:
            return handle_diagnostic_session_control(server, request, len, now);
            
        case UDS_SID_SECURITY_ACCESS:
            return handle_security_access(server, request, len, now);
            
        case UDS_SID_TESTER_PRESENT:
            return handle_tester_present(server, request, len, now);
            
        case UDS_SID_READ_DATA_BY_IDENTIFIER:
            return handle_read_data_by_identifier(server, request, len);
            
        case UDS_SID_WRITE_DATA_BY_IDENTIFIER:
            return handle_write_data_by_identifier(server, request, len);
            
        case UDS_SID_ROUTINE_CONTROL:
            return handle_routine_control(server, request, len);
            
        default:
            return send_negative_response(server, sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
}

void lq_uds_server_periodic(struct lq_uds_server *server, uint64_t now)
{
    if (!server) {
        return;
    }
    
    /* Check S3 timeout (session timeout) */
    if (server->current_session != UDS_SESSION_DEFAULT) {
        uint64_t inactive_time = now - server->last_activity_time;
        uint64_t s3_timeout_us = server->config.s3_server * 1000ULL;
        
        if (inactive_time > s3_timeout_us) {
            /* Session timeout - revert to default session */
            LQ_UDS_LOG("S3 timeout - reverting to default session");
            server->current_session = UDS_SESSION_DEFAULT;
            server->security_level = UDS_SECURITY_LOCKED;
        }
    }
}

enum lq_uds_session lq_uds_get_session(const struct lq_uds_server *server)
{
    return server ? server->current_session : UDS_SESSION_DEFAULT;
}

enum lq_uds_security_level lq_uds_get_security_level(const struct lq_uds_server *server)
{
    return server ? server->security_level : UDS_SECURITY_LOCKED;
}

bool lq_uds_is_service_allowed(const struct lq_uds_server *server, uint8_t sid)
{
    if (!server) {
        return false;
    }
    
    return check_session_for_service(server, sid);
}

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

size_t lq_uds_build_positive_response(uint8_t *response, uint8_t sid,
                                       const uint8_t *data, size_t data_len)
{
    if (!response) {
        return 0;
    }
    
    response[0] = sid + UDS_POSITIVE_RESPONSE_OFFSET;
    
    if (data && data_len > 0) {
        memcpy(&response[1], data, data_len);
        return 1 + data_len;
    }
    
    return 1;
}

size_t lq_uds_build_negative_response(uint8_t *response, uint8_t sid, uint8_t nrc)
{
    if (!response) {
        return 0;
    }
    
    response[0] = UDS_SID_NEGATIVE_RESPONSE;
    response[1] = sid;
    response[2] = nrc;
    
    return 3;
}
