/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * UDS (Unified Diagnostic Services) Tests
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "lq_uds.h"
}

/* ============================================================================
 * Mock Transport Layer
 * ============================================================================ */

static uint8_t mock_tx_buffer[4096];
static size_t mock_tx_len = 0;

static int mock_transport_send(void *ctx, const uint8_t *data, size_t len)
{
    (void)ctx;
    if (len > sizeof(mock_tx_buffer)) {
        return -1;
    }
    memcpy(mock_tx_buffer, data, len);
    mock_tx_len = len;
    return 0;
}

static size_t mock_transport_get_max_payload(void *ctx)
{
    (void)ctx;
    return 4095;
}

static const struct lq_uds_transport_ops mock_transport_ops = {
    .send = mock_transport_send,
    .get_max_payload = mock_transport_get_max_payload
};

static struct lq_uds_transport mock_transport = {
    .ops = &mock_transport_ops,
    .ctx = nullptr
};

/* ============================================================================
 * Mock Security Callbacks
 * ============================================================================ */

static const uint8_t test_seed[] = {0x12, 0x34, 0x56, 0x78};
static const uint8_t test_key[] = {0x9A, 0xBC, 0xDE, 0xF0};

static int mock_get_seed(uint8_t level, uint8_t *seed)
{
    (void)level;
    memcpy(seed, test_seed, 4);
    return 0;
}

static bool mock_verify_key(uint8_t level, const uint8_t *key)
{
    (void)level;
    return memcmp(key, test_key, 4) == 0;
}

/* ============================================================================
 * Mock DID Callbacks
 * ============================================================================ */

static int mock_read_did(uint16_t did, uint8_t *data, size_t max_len, size_t *actual_len)
{
    /* Example: VIN */
    if (did == UDS_DID_VIN) {
        const char *vin = "1HGBH41JXMN109186";
        size_t vin_len = strlen(vin);
        if (max_len < vin_len) {
            return -UDS_NRC_RESPONSE_TOO_LONG;
        }
        memcpy(data, vin, vin_len);
        *actual_len = vin_len;
        return 0;
    }
    
    /* Unknown DID */
    return -UDS_NRC_REQUEST_OUT_OF_RANGE;
}

static int mock_write_did(uint16_t did, const uint8_t *data, size_t len)
{
    /* Example: Allow writing custom DID */
    if (did == UDS_DID_LQ_REMAP_CONFIG && len == 4) {
        return 0;  /* Success */
    }
    
    return -UDS_NRC_REQUEST_OUT_OF_RANGE;
}

/* ============================================================================
 * Mock Routine Control
 * ============================================================================ */

static int mock_routine_control(uint16_t rid, uint8_t control_type,
                                  const uint8_t *in_data, size_t in_len,
                                  uint8_t *out_data, size_t max_out, size_t *actual_out)
{
    (void)in_data;
    (void)in_len;
    
    /* Calibration mode routine */
    if (rid == UDS_RID_ENTER_CALIBRATION_MODE && control_type == UDS_ROUTINE_START) {
        /* Return success status */
        if (max_out < 1) {
            return -UDS_NRC_RESPONSE_TOO_LONG;
        }
        out_data[0] = 0x00;  /* Success */
        *actual_out = 1;
        return 0;
    }
    
    return -UDS_NRC_REQUEST_OUT_OF_RANGE;
}

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class UDSTest : public ::testing::Test {
protected:
    struct lq_uds_server server;
    struct lq_uds_config config;
    
    void SetUp() override {
        /* Clear mock */
        mock_tx_len = 0;
        memset(mock_tx_buffer, 0, sizeof(mock_tx_buffer));
        
        /* Configure UDS server */
        memset(&config, 0, sizeof(config));
        config.transport = &mock_transport;
        config.get_seed = mock_get_seed;
        config.verify_key = mock_verify_key;
        config.read_did = mock_read_did;
        config.write_did = mock_write_did;
        config.routine_control = mock_routine_control;
        
        /* Initialize */
        ASSERT_EQ(lq_uds_server_init(&server, &config), 0);
    }
    
    void TearDown() override {
        /* Cleanup if needed */
    }
    
    /* Helper to send request and get response */
    void SendRequest(const uint8_t *request, size_t len, uint64_t now = 0) {
        mock_tx_len = 0;
        lq_uds_server_process_request(&server, request, len, now);
    }
    
    /* Helpers to check response */
    bool IsPositiveResponse(uint8_t sid) {
        return mock_tx_len > 0 && mock_tx_buffer[0] == (sid + UDS_POSITIVE_RESPONSE_OFFSET);
    }
    
    bool IsNegativeResponse(uint8_t sid, uint8_t nrc) {
        return mock_tx_len == 3 &&
               mock_tx_buffer[0] == UDS_SID_NEGATIVE_RESPONSE &&
               mock_tx_buffer[1] == sid &&
               mock_tx_buffer[2] == nrc;
    }
};

/* ============================================================================
 * Diagnostic Session Control Tests
 * ============================================================================ */

TEST_F(UDSTest, SessionControl_SwitchToExtended) {
    uint8_t request[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL));
    EXPECT_EQ(mock_tx_buffer[1], UDS_SESSION_EXTENDED_DIAGNOSTIC);
    EXPECT_EQ(lq_uds_get_session(&server), UDS_SESSION_EXTENDED_DIAGNOSTIC);
}

TEST_F(UDSTest, SessionControl_SwitchToProgramming) {
    uint8_t request[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_PROGRAMMING};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL));
    EXPECT_EQ(lq_uds_get_session(&server), UDS_SESSION_PROGRAMMING);
}

TEST_F(UDSTest, SessionControl_InvalidSessionType) {
    uint8_t request[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, 0xFF};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL, 
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED));
}

TEST_F(UDSTest, SessionControl_IncorrectLength) {
    uint8_t request[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_DIAGNOSTIC_SESSION_CONTROL,
                                    UDS_NRC_INCORRECT_MESSAGE_LENGTH));
}

TEST_F(UDSTest, SessionControl_TimingParameters) {
    uint8_t request[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(request, sizeof(request));
    
    /* Check P2 and P2* timing in response */
    EXPECT_EQ(mock_tx_len, 6);
    uint16_t p2 = ((uint16_t)mock_tx_buffer[2] << 8) | mock_tx_buffer[3];
    uint16_t p2_star = ((uint16_t)mock_tx_buffer[4] << 8) | mock_tx_buffer[5];
    
    EXPECT_EQ(p2, 5);      /* 50ms / 10 */
    EXPECT_EQ(p2_star, 500); /* 5000ms / 10 */
}

/* ============================================================================
 * Security Access Tests
 * ============================================================================ */

TEST_F(UDSTest, SecurityAccess_RequestSeed) {
    /* First switch to extended session */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    /* Request seed for level 1 */
    uint8_t request[] = {UDS_SID_SECURITY_ACCESS, 0x01};  /* RequestSeed level 1 */
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_SECURITY_ACCESS));
    EXPECT_EQ(mock_tx_len, 6);
    EXPECT_EQ(mock_tx_buffer[1], 0x01);
    /* Check seed matches */
    EXPECT_EQ(memcmp(&mock_tx_buffer[2], test_seed, 4), 0);
}

TEST_F(UDSTest, SecurityAccess_SendKey_Valid) {
    /* Switch to extended session and request seed */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    /* Send correct key */
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02, 
                         test_key[0], test_key[1], test_key[2], test_key[3]};
    SendRequest(key_req, sizeof(key_req));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_SECURITY_ACCESS));
    EXPECT_EQ(lq_uds_get_security_level(&server), UDS_SECURITY_LEVEL_1);
}

TEST_F(UDSTest, SecurityAccess_SendKey_Invalid) {
    /* Setup */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    /* Send incorrect key */
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02, 0x00, 0x00, 0x00, 0x00};
    SendRequest(key_req, sizeof(key_req));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_SECURITY_ACCESS, UDS_NRC_INVALID_KEY));
    EXPECT_EQ(lq_uds_get_security_level(&server), UDS_SECURITY_LOCKED);
}

TEST_F(UDSTest, SecurityAccess_AlreadyUnlocked_ZeroSeed) {
    /* Unlock first */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02,
                         test_key[0], test_key[1], test_key[2], test_key[3]};
    SendRequest(key_req, sizeof(key_req));
    
    /* Request seed again - should get zero seed */
    SendRequest(seed_req, sizeof(seed_req));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_SECURITY_ACCESS));
    uint8_t zero_seed[] = {0, 0, 0, 0};
    EXPECT_EQ(memcmp(&mock_tx_buffer[2], zero_seed, 4), 0);
}

TEST_F(UDSTest, SecurityAccess_MaxAttempts_Lockout) {
    /* Setup */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    uint8_t bad_key[] = {UDS_SID_SECURITY_ACCESS, 0x02, 0x00, 0x00, 0x00, 0x00};
    
    /* Exhaust attempts */
    for (int i = 0; i < 3; i++) {
        SendRequest(seed_req, sizeof(seed_req));
        SendRequest(bad_key, sizeof(bad_key));
    }
    
    /* Next attempt should trigger lockout */
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_SECURITY_ACCESS, 
                                    UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS));
}

/* ============================================================================
 * Tester Present Tests
 * ============================================================================ */

TEST_F(UDSTest, TesterPresent_KeepSessionAlive) {
    uint8_t request[] = {UDS_SID_TESTER_PRESENT, 0x00};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_TESTER_PRESENT));
    EXPECT_EQ(mock_tx_buffer[1], 0x00);
}

TEST_F(UDSTest, TesterPresent_SuppressResponse) {
    uint8_t request[] = {UDS_SID_TESTER_PRESENT, 0x80};  /* Suppress positive response bit */
    SendRequest(request, sizeof(request));
    
    /* No response expected */
    EXPECT_EQ(mock_tx_len, 0);
}

TEST_F(UDSTest, TesterPresent_InvalidSubfunction) {
    uint8_t request[] = {UDS_SID_TESTER_PRESENT, 0x01};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_TESTER_PRESENT,
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED));
}

/* ============================================================================
 * Read Data By Identifier Tests
 * ============================================================================ */

TEST_F(UDSTest, ReadDID_VIN_Success) {
    uint8_t request[] = {UDS_SID_READ_DATA_BY_IDENTIFIER, 0xF1, 0x90};  /* VIN */
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_READ_DATA_BY_IDENTIFIER));
    EXPECT_EQ(mock_tx_buffer[1], 0xF1);
    EXPECT_EQ(mock_tx_buffer[2], 0x90);
    
    /* Check VIN data */
    const char *vin = "1HGBH41JXMN109186";
    EXPECT_EQ(memcmp(&mock_tx_buffer[3], vin, strlen(vin)), 0);
}

TEST_F(UDSTest, ReadDID_UnknownDID) {
    uint8_t request[] = {UDS_SID_READ_DATA_BY_IDENTIFIER, 0xFF, 0xFF};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_READ_DATA_BY_IDENTIFIER,
                                    UDS_NRC_REQUEST_OUT_OF_RANGE));
}

TEST_F(UDSTest, ReadDID_IncorrectLength) {
    uint8_t request[] = {UDS_SID_READ_DATA_BY_IDENTIFIER, 0xF1};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_READ_DATA_BY_IDENTIFIER,
                                    UDS_NRC_INCORRECT_MESSAGE_LENGTH));
}

/* ============================================================================
 * Write Data By Identifier Tests
 * ============================================================================ */

TEST_F(UDSTest, WriteDID_RequiresExtendedSession) {
    /* Try writing in default session */
    uint8_t request[] = {UDS_SID_WRITE_DATA_BY_IDENTIFIER, 0xF1, 0xA2, 0x01, 0x02, 0x03, 0x04};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                                    UDS_NRC_SERVICE_NOT_SUPPORTED_IN_SESSION));
}

TEST_F(UDSTest, WriteDID_RequiresSecurity) {
    /* Switch to extended session */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    /* Try writing without security access */
    uint8_t request[] = {UDS_SID_WRITE_DATA_BY_IDENTIFIER, 0xF1, 0xA2, 0x01, 0x02, 0x03, 0x04};
    SendRequest(request, sizeof(request));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_WRITE_DATA_BY_IDENTIFIER,
                                    UDS_NRC_SECURITY_ACCESS_DENIED));
}

TEST_F(UDSTest, WriteDID_Success) {
    /* Setup: extended session + security */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02,
                         test_key[0], test_key[1], test_key[2], test_key[3]};
    SendRequest(key_req, sizeof(key_req));
    
    /* Now write */
    uint8_t write_req[] = {UDS_SID_WRITE_DATA_BY_IDENTIFIER, 0xF1, 0xA2, 0x01, 0x02, 0x03, 0x04};
    SendRequest(write_req, sizeof(write_req));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_WRITE_DATA_BY_IDENTIFIER));
    EXPECT_EQ(mock_tx_buffer[1], 0xF1);
    EXPECT_EQ(mock_tx_buffer[2], 0xA2);
}

/* ============================================================================
 * Routine Control Tests
 * ============================================================================ */

TEST_F(UDSTest, RoutineControl_StartCalibrationMode) {
    /* Setup: extended session + security */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02,
                         test_key[0], test_key[1], test_key[2], test_key[3]};
    SendRequest(key_req, sizeof(key_req));
    
    /* Start calibration mode routine */
    uint8_t routine_req[] = {UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_START, 0xF1, 0xA0};
    SendRequest(routine_req, sizeof(routine_req));
    
    EXPECT_TRUE(IsPositiveResponse(UDS_SID_ROUTINE_CONTROL));
    EXPECT_EQ(mock_tx_buffer[1], UDS_ROUTINE_START);
    EXPECT_EQ(mock_tx_buffer[2], 0xF1);
    EXPECT_EQ(mock_tx_buffer[3], 0xA0);
    EXPECT_EQ(mock_tx_buffer[4], 0x00);  /* Success status */
}

TEST_F(UDSTest, RoutineControl_RequiresSecurity) {
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t routine_req[] = {UDS_SID_ROUTINE_CONTROL, UDS_ROUTINE_START, 0xF1, 0xA0};
    SendRequest(routine_req, sizeof(routine_req));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_ROUTINE_CONTROL,
                                    UDS_NRC_SECURITY_ACCESS_DENIED));
}

TEST_F(UDSTest, RoutineControl_InvalidControlType) {
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req));
    
    uint8_t seed_req[] = {UDS_SID_SECURITY_ACCESS, 0x01};
    SendRequest(seed_req, sizeof(seed_req));
    
    uint8_t key_req[] = {UDS_SID_SECURITY_ACCESS, 0x02,
                         test_key[0], test_key[1], test_key[2], test_key[3]};
    SendRequest(key_req, sizeof(key_req));
    
    uint8_t routine_req[] = {UDS_SID_ROUTINE_CONTROL, 0xFF, 0xF1, 0xA0};
    SendRequest(routine_req, sizeof(routine_req));
    
    EXPECT_TRUE(IsNegativeResponse(UDS_SID_ROUTINE_CONTROL,
                                    UDS_NRC_SUBFUNCTION_NOT_SUPPORTED));
}

/* ============================================================================
 * Session Timeout Tests
 * ============================================================================ */

TEST_F(UDSTest, SessionTimeout_S3Timeout) {
    /* Switch to extended session */
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req), 0);
    
    EXPECT_EQ(lq_uds_get_session(&server), UDS_SESSION_EXTENDED_DIAGNOSTIC);
    
    /* Simulate S3 timeout (5 seconds + 1ms) */
    lq_uds_server_periodic(&server, 5001000ULL);
    
    /* Should revert to default session */
    EXPECT_EQ(lq_uds_get_session(&server), UDS_SESSION_DEFAULT);
    EXPECT_EQ(lq_uds_get_security_level(&server), UDS_SECURITY_LOCKED);
}

TEST_F(UDSTest, SessionTimeout_TesterPresent_PreventsTimeout) {
    uint8_t session_req[] = {UDS_SID_DIAGNOSTIC_SESSION_CONTROL, UDS_SESSION_EXTENDED_DIAGNOSTIC};
    SendRequest(session_req, sizeof(session_req), 0);
    
    /* Send tester present before timeout */
    uint8_t tester_req[] = {UDS_SID_TESTER_PRESENT, 0x00};
    SendRequest(tester_req, sizeof(tester_req), 4000000ULL);  /* 4 seconds */
    
    /* Check timeout after tester present */
    lq_uds_server_periodic(&server, 6000000ULL);  /* 6 seconds total */
    
    /* Should still be in extended session (4s + 2s < 5s from last activity) */
    EXPECT_EQ(lq_uds_get_session(&server), UDS_SESSION_EXTENDED_DIAGNOSTIC);
}

/* ============================================================================
 * Helper Function Tests
 * ============================================================================ */

TEST_F(UDSTest, BuildPositiveResponse) {
    uint8_t response[10];
    uint8_t data[] = {0x01, 0x02, 0x03};
    
    size_t len = lq_uds_build_positive_response(response, 0x22, data, 3);
    
    EXPECT_EQ(len, 4);
    EXPECT_EQ(response[0], 0x62);  /* 0x22 + 0x40 */
    EXPECT_EQ(response[1], 0x01);
    EXPECT_EQ(response[2], 0x02);
    EXPECT_EQ(response[3], 0x03);
}

TEST_F(UDSTest, BuildNegativeResponse) {
    uint8_t response[3];
    
    size_t len = lq_uds_build_negative_response(response, 0x22, UDS_NRC_REQUEST_OUT_OF_RANGE);
    
    EXPECT_EQ(len, 3);
    EXPECT_EQ(response[0], UDS_SID_NEGATIVE_RESPONSE);
    EXPECT_EQ(response[1], 0x22);
    EXPECT_EQ(response[2], UDS_NRC_REQUEST_OUT_OF_RANGE);
}
