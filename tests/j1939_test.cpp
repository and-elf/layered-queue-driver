/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Google Test tests for J1939 Protocol Driver
 * Comprehensive coverage for production validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
#include "lq_j1939.h"
#include "lq_protocol.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class J1939Test : public ::testing::Test {
protected:
    struct lq_j1939_ctx ctx;
    struct lq_protocol_driver proto;
    struct lq_protocol_config config;
    
    // Static buffers for messages (no malloc)
    uint8_t msg_buffer[8];
    uint32_t signal_ids[16];
    struct lq_protocol_decode_map decode_maps[4];
    struct lq_protocol_encode_map encode_maps[4];
    
    void SetUp() override {
        memset(&ctx, 0, sizeof(ctx));
        memset(&proto, 0, sizeof(proto));
        memset(&config, 0, sizeof(config));
        memset(msg_buffer, 0, sizeof(msg_buffer));
        
        // Setup basic protocol driver
        proto.vtbl = &lq_j1939_protocol_vtbl;
        proto.ctx = &ctx;
        
        // Basic config
        config.node_address = 0x25;  // Standard ECU address
        config.decode_maps = decode_maps;
        config.encode_maps = encode_maps;
        config.num_decode_maps = 0;
        config.num_encode_maps = 0;
    }
    
    void TearDown() override {
        // No dynamic allocation, nothing to free
    }
    
    // Helper to create a J1939 CAN ID
    uint32_t make_can_id(uint8_t priority, uint32_t pgn, uint8_t sa) {
        return lq_j1939_build_id_from_pgn(pgn, priority, sa);
    }
    
    // Helper to create protocol message
    struct lq_protocol_msg make_msg(uint32_t can_id, const uint8_t* data, size_t len) {
        struct lq_protocol_msg msg;
        msg.address = can_id;
        msg.data = const_cast<uint8_t*>(data);
        msg.len = len;
        msg.capacity = len;
        msg.timestamp = 1000000;  // 1 second
        msg.flags = 0;
        return msg;
    }
};

// ============================================================================
// Initialization Tests
// ============================================================================

TEST_F(J1939Test, InitBasic) {
    int ret = proto.vtbl->init(&proto, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ctx.node_address, 0x25);
    EXPECT_EQ(ctx.num_signals, 0);
    EXPECT_EQ(ctx.num_cyclic, 0);
}

TEST_F(J1939Test, InitNullProtocol) {
    int ret = proto.vtbl->init(nullptr, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(J1939Test, InitNullConfig) {
    int ret = proto.vtbl->init(&proto, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(J1939Test, InitWithEncodeMaps) {
    // Setup encode maps for cyclic transmission
    encode_maps[0].protocol_id = J1939_PGN_EEC1;
    encode_maps[0].period_ms = 50;
    encode_maps[1].protocol_id = J1939_PGN_ET1;
    encode_maps[1].period_ms = 1000;
    config.num_encode_maps = 2;
    
    int ret = proto.vtbl->init(&proto, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ctx.num_cyclic, 2);
    EXPECT_EQ(ctx.cyclic_msgs[0].pgn, J1939_PGN_EEC1);
    EXPECT_EQ(ctx.cyclic_msgs[0].period_ms, 50);
    EXPECT_EQ(ctx.cyclic_msgs[1].pgn, J1939_PGN_ET1);
    EXPECT_EQ(ctx.cyclic_msgs[1].period_ms, 1000);
}

// ============================================================================
// CAN ID Construction Tests
// ============================================================================

TEST_F(J1939Test, BuildCanIdFromPgn) {
    uint32_t can_id = lq_j1939_build_id_from_pgn(J1939_PGN_EEC1, 3, 0x25);
    
    // Extract fields
    uint8_t priority = (can_id >> 26) & 0x7;
    uint32_t pgn = lq_j1939_extract_pgn(can_id);
    uint8_t sa = can_id & 0xFF;
    
    EXPECT_EQ(priority, 3);
    EXPECT_EQ(pgn, J1939_PGN_EEC1);
    EXPECT_EQ(sa, 0x25);
}

TEST_F(J1939Test, ExtractPgn) {
    uint32_t can_id = lq_j1939_build_id_from_pgn(J1939_PGN_DM1, 6, 0x00);
    uint32_t pgn = lq_j1939_extract_pgn(can_id);
    EXPECT_EQ(pgn, J1939_PGN_DM1);
}

TEST_F(J1939Test, PgnConstantsValid) {
    EXPECT_EQ(J1939_PGN_EEC1, 65265);
    EXPECT_EQ(J1939_PGN_ET1, 65262);
    EXPECT_EQ(J1939_PGN_DM1, 65226);
}

// ============================================================================
// Decode Tests - EEC1 (Engine Controller)
// ============================================================================

TEST_F(J1939Test, DecodeEEC1_Basic) {
    // Setup decode map for EEC1
    signal_ids[0] = 1000;  // RPM signal
    signal_ids[1] = 1001;  // Torque signal
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 2;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // Create EEC1 message: 1500 RPM, 75% torque
    // RPM encoding: 188 (value @ 0.125 rpm/bit = 23.5 actual RPM, rounds to 23)
    // Torque encoding: 200 (value @ 1%/bit, offset -125 = 75% actual)
    uint8_t data[8] = {0xFF, 0xFF, 200, 188, 0, 0xFF, 0xFF, 0xFF};
    // Byte 2: torque = 200
    // Bytes 3-4: RPM little-endian = 188, 0 -> 0x00BC = 188
    
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x10);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 2);
    EXPECT_EQ(events[0].source_id, 1000);  // RPM
    EXPECT_EQ(events[0].value, 23);        // 188 * 0.125 = 23.5, truncated
    EXPECT_EQ(events[0].status, LQ_EVENT_OK);
    
    EXPECT_EQ(events[1].source_id, 1001);  // Torque
    EXPECT_EQ(events[1].value, 75);        // 200 - 125 = 75
    EXPECT_EQ(events[1].status, LQ_EVENT_OK);
}

TEST_F(J1939Test, DecodeEEC1_HighRPM) {
    signal_ids[0] = 1000;
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // 2000 RPM = 2000 / 0.125 = 16000 = 0x3E80
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0x80, 0x3E, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x00);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 1);
    EXPECT_EQ(events[0].value, 2000);
}

TEST_F(J1939Test, DecodeEEC1_OnlyOneSignalMapped) {
    // Map only RPM signal (first signal is always RPM for EEC1)
    signal_ids[0] = 1000;
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // RPM value 200 @ 0.125 rpm/bit = 25 RPM
    uint8_t data[8] = {0xFF, 0xFF, 150, 200, 0, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x00);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    // Should only decode RPM (first signal in map)
    EXPECT_EQ(num_events, 1);
    EXPECT_EQ(events[0].source_id, 1000);
    EXPECT_EQ(events[0].value, 25);  // 200 * 0.125 = 25
}

// ============================================================================
// Decode Tests - ET1 (Engine Temperature)
// ============================================================================

TEST_F(J1939Test, DecodeET1_NormalTemp) {
    signal_ids[0] = 2000;  // Temperature signal
    decode_maps[0].protocol_id = J1939_PGN_ET1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // 90°C = 90 + 40 = 130
    uint8_t data[8] = {130, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_ET1, 0x00);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 1);
    EXPECT_EQ(events[0].source_id, 2000);
    EXPECT_EQ(events[0].value, 90);  // 130 - 40 = 90°C
}

TEST_F(J1939Test, DecodeET1_NegativeTemp) {
    signal_ids[0] = 2000;
    decode_maps[0].protocol_id = J1939_PGN_ET1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // -10°C = -10 + 40 = 30
    uint8_t data[8] = {30, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_ET1, 0x00);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 1);
    EXPECT_EQ(events[0].value, -10);
}

// ============================================================================
// Decode Error Handling Tests
// ============================================================================

TEST_F(J1939Test, DecodeNullProtocol) {
    uint8_t data[8] = {0};
    auto msg = make_msg(0, data, 8);
    struct lq_event events[10];
    
    size_t num_events = proto.vtbl->decode(nullptr, 1000000, &msg, events, 10);
    EXPECT_EQ(num_events, 0);
}

TEST_F(J1939Test, DecodeNullMessage) {
    proto.vtbl->init(&proto, &config);
    struct lq_event events[10];
    
    size_t num_events = proto.vtbl->decode(&proto, 1000000, nullptr, events, 10);
    EXPECT_EQ(num_events, 0);
}

TEST_F(J1939Test, DecodeNullEvents) {
    proto.vtbl->init(&proto, &config);
    uint8_t data[8] = {0};
    auto msg = make_msg(0, data, 8);
    
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, nullptr, 10);
    EXPECT_EQ(num_events, 0);
}

TEST_F(J1939Test, DecodeMaxEventsZero) {
    proto.vtbl->init(&proto, &config);
    uint8_t data[8] = {0};
    auto msg = make_msg(0, data, 8);
    struct lq_event events[10];
    
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 0);
    EXPECT_EQ(num_events, 0);
}

TEST_F(J1939Test, DecodeTooShort) {
    signal_ids[0] = 1000;
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // Only 4 bytes instead of 8
    uint8_t data[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x00);
    auto msg = make_msg(can_id, data, 4);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 0);  // Should reject short messages
}

TEST_F(J1939Test, DecodeUnmappedPGN) {
    // No decode maps configured
    proto.vtbl->init(&proto, &config);
    
    uint8_t data[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x00);
    auto msg = make_msg(can_id, data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 0);  // Unmapped PGN ignored
}

TEST_F(J1939Test, DecodeNullDataPointer) {
    signal_ids[0] = 1000;
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 1;
    config.num_decode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x00);
    struct lq_protocol_msg msg;
    msg.address = can_id;
    msg.data = nullptr;  // Null data
    msg.len = 8;
    msg.capacity = 8;
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num_events, 0);
}

// ============================================================================
// Update Signal Tests
// ============================================================================

TEST_F(J1939Test, UpdateSignalBasic) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 1000, 1500, 1000000);
    
    EXPECT_EQ(ctx.num_signals, 1);
    EXPECT_EQ(ctx.signals[0].signal_id, 1000);
    EXPECT_EQ(ctx.signals[0].value, 1500);
    EXPECT_EQ(ctx.signals[0].timestamp, 1000000);
}

TEST_F(J1939Test, UpdateSignalMultiple) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 1000, 1500, 1000000);
    proto.vtbl->update_signal(&proto, 1001, 75, 1000000);
    proto.vtbl->update_signal(&proto, 1002, 90, 1000000);
    
    EXPECT_EQ(ctx.num_signals, 3);
    EXPECT_EQ(ctx.signals[0].signal_id, 1000);
    EXPECT_EQ(ctx.signals[1].signal_id, 1001);
    EXPECT_EQ(ctx.signals[2].signal_id, 1002);
}

TEST_F(J1939Test, UpdateSignalOverwrite) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 1000, 1500, 1000000);
    proto.vtbl->update_signal(&proto, 1000, 2000, 2000000);
    
    EXPECT_EQ(ctx.num_signals, 1);  // Same signal, not added twice
    EXPECT_EQ(ctx.signals[0].value, 2000);  // Updated value
    EXPECT_EQ(ctx.signals[0].timestamp, 2000000);
}

TEST_F(J1939Test, UpdateSignalMaxCapacity) {
    proto.vtbl->init(&proto, &config);
    
    // Fill to capacity (32 signals)
    for (uint32_t i = 0; i < 32; i++) {
        proto.vtbl->update_signal(&proto, i, i * 10, 1000000);
    }
    
    EXPECT_EQ(ctx.num_signals, 32);
    
    // Try to add one more (should be ignored)
    proto.vtbl->update_signal(&proto, 100, 999, 1000000);
    
    EXPECT_EQ(ctx.num_signals, 32);  // Still 32, not 33
}

TEST_F(J1939Test, UpdateSignalNullProtocol) {
    // Should not crash
    proto.vtbl->update_signal(nullptr, 1000, 1500, 1000000);
}

// ============================================================================
// Cyclic Transmission Tests
// ============================================================================

TEST_F(J1939Test, GetCyclicNoMessages) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg msgs[10];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = msg_buffer;
        msgs[i].capacity = 8;
    }
    
    size_t num_msgs = proto.vtbl->get_cyclic(&proto, 1000000, msgs, 10);
    
    EXPECT_EQ(num_msgs, 0);  // No encode maps configured
}

TEST_F(J1939Test, GetCyclicFirstTransmission) {
    // Configure EEC1 with 50ms period
    encode_maps[0].protocol_id = J1939_PGN_EEC1;
    encode_maps[0].period_ms = 50;
    config.num_encode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // Add signal data
    proto.vtbl->update_signal(&proto, 1000, 1500, 0);  // RPM
    proto.vtbl->update_signal(&proto, 1001, 75, 0);    // Torque
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // First call should transmit immediately (last_tx_time = 0)
    size_t num_msgs = proto.vtbl->get_cyclic(&proto, 50000, msgs, 10);
    
    EXPECT_EQ(num_msgs, 1);
    EXPECT_EQ(lq_j1939_extract_pgn(msgs[0].address), J1939_PGN_EEC1);
    EXPECT_EQ(msgs[0].len, 8);
}

TEST_F(J1939Test, GetCyclicTimingRespected) {
    encode_maps[0].protocol_id = J1939_PGN_EEC1;
    encode_maps[0].period_ms = 100;  // 100ms period
    config.num_encode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // First transmission at 100ms (initial last_tx_time=0, elapsed=100ms >= 100ms period)
    size_t num = proto.vtbl->get_cyclic(&proto, 100000, msgs, 10);
    EXPECT_EQ(num, 1);
    
    // Try again at 150ms - should not transmit (only 50ms elapsed since last TX)
    num = proto.vtbl->get_cyclic(&proto, 150000, msgs, 10);
    EXPECT_EQ(num, 0);
    
    // Try at 199ms - still should not transmit (99ms since last TX)
    num = proto.vtbl->get_cyclic(&proto, 199000, msgs, 10);
    EXPECT_EQ(num, 0);
    
    // Try at 200ms - should transmit (100ms since first TX @ 100ms)
    num = proto.vtbl->get_cyclic(&proto, 200000, msgs, 10);
    EXPECT_EQ(num, 1);
}

TEST_F(J1939Test, GetCyclicMultipleMessages) {
    encode_maps[0].protocol_id = J1939_PGN_EEC1;
    encode_maps[0].period_ms = 50;
    encode_maps[1].protocol_id = J1939_PGN_ET1;
    encode_maps[1].period_ms = 100;
    config.num_encode_maps = 2;
    
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // At 100ms, both should transmit
    size_t num = proto.vtbl->get_cyclic(&proto, 100000, msgs, 10);
    
    EXPECT_EQ(num, 2);
    // Order may vary, check both PGNs present
    std::vector<uint32_t> pgns;
    pgns.push_back(lq_j1939_extract_pgn(msgs[0].address));
    pgns.push_back(lq_j1939_extract_pgn(msgs[1].address));
    
    EXPECT_TRUE(std::find(pgns.begin(), pgns.end(), J1939_PGN_EEC1) != pgns.end());
    EXPECT_TRUE(std::find(pgns.begin(), pgns.end(), J1939_PGN_ET1) != pgns.end());
}

TEST_F(J1939Test, GetCyclicNullProtocol) {
    struct lq_protocol_msg msgs[10];
    size_t num = proto.vtbl->get_cyclic(nullptr, 1000000, msgs, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(J1939Test, GetCyclicNullMessages) {
    proto.vtbl->init(&proto, &config);
    size_t num = proto.vtbl->get_cyclic(&proto, 1000000, nullptr, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(J1939Test, GetCyclicMaxMessagesZero) {
    proto.vtbl->init(&proto, &config);
    struct lq_protocol_msg msgs[10];
    size_t num = proto.vtbl->get_cyclic(&proto, 1000000, msgs, 0);
    EXPECT_EQ(num, 0);
}

// ============================================================================
// DM1 (Diagnostic Trouble Codes) Tests
// ============================================================================

TEST_F(J1939Test, FormatDM1_Empty) {
    lq_j1939_dm1_t dm1;
    memset(&dm1, 0, sizeof(dm1));
    dm1.malfunction_lamp = J1939_LAMP_OFF;
    dm1.dtc_count = 0;
    
    uint8_t data[8];
    int ret = lq_j1939_format_dm1(&dm1, data, 8);
    
    EXPECT_EQ(ret, 0);
    // Check lamp status byte
    EXPECT_EQ(data[0] & 0x03, J1939_LAMP_OFF);
}

TEST_F(J1939Test, FormatDM1_WithDTC) {
    lq_j1939_dm1_t dm1;
    memset(&dm1, 0, sizeof(dm1));
    dm1.malfunction_lamp = J1939_LAMP_ON;
    dm1.red_stop_lamp = J1939_LAMP_SLOW_FLASH;
    
    // Create a DTC: SPN=1234, FMI=5, OC=1
    dm1.dtc_list[0] = lq_j1939_create_dtc(1234, 5, 1);
    dm1.dtc_count = 1;
    
    uint8_t data[8];
    int ret = lq_j1939_format_dm1(&dm1, data, 8);
    
    EXPECT_EQ(ret, 0);
    
    // Verify lamp status (byte 0 encoding):
    // Bits 0-1: protect_lamp
    // Bits 2-3: amber_warning_lamp
    // Bits 4-5: red_stop_lamp
    // Bits 6-7: malfunction_lamp
    EXPECT_EQ((data[0] >> 6) & 0x03, J1939_LAMP_ON);         // Malfunction lamp
    EXPECT_EQ((data[0] >> 4) & 0x03, J1939_LAMP_SLOW_FLASH); // Red stop lamp
    
    // Verify DTC encoding
    uint32_t spn = lq_j1939_get_spn(dm1.dtc_list[0]);
    uint8_t fmi = lq_j1939_get_fmi(dm1.dtc_list[0]);
    uint8_t oc = lq_j1939_get_oc(dm1.dtc_list[0]);
    
    EXPECT_EQ(spn, 1234);
    EXPECT_EQ(fmi, 5);
    EXPECT_EQ(oc, 1);
}

TEST_F(J1939Test, DecodeDM1_Basic) {
    uint8_t data[8] = {
        0x00,        // Lamps off
        0xFF,        // Flash bits
        0xD2, 0x04,  // SPN low bytes (1234 = 0x04D2)
        0x28,        // SPN high + FMI (5 = 0b00101)
        0x04,        // FMI high + OC (1 = 0b0000001)
        0xFF, 0xFF
    };
    
    lq_j1939_dm1_t dm1;
    int ret = lq_j1939_decode_dm1(data, 8, &dm1);
    
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(dm1.dtc_count, 1);
    
    uint32_t spn = lq_j1939_get_spn(dm1.dtc_list[0]);
    EXPECT_EQ(spn, 1234);
}

// ============================================================================
// DTC Helper Functions Tests
// ============================================================================

TEST_F(J1939Test, CreateAndExtractDTC) {
    uint32_t dtc = lq_j1939_create_dtc(1234, 5, 1);
    
    EXPECT_EQ(lq_j1939_get_spn(dtc), 1234);
    EXPECT_EQ(lq_j1939_get_fmi(dtc), 5);
    EXPECT_EQ(lq_j1939_get_oc(dtc), 1);
}

TEST_F(J1939Test, CreateDTCMaxValues) {
    // SPN max: 19 bits = 524287
    // FMI max: 5 bits = 31
    // OC max: 7 bits = 127
    uint32_t dtc = lq_j1939_create_dtc(524287, 31, 127);
    
    EXPECT_EQ(lq_j1939_get_spn(dtc), 524287);
    EXPECT_EQ(lq_j1939_get_fmi(dtc), 31);
    EXPECT_EQ(lq_j1939_get_oc(dtc), 127);
}

TEST_F(J1939Test, CreateDTCCommonSPNs) {
    // Test some common SAE J1939 SPNs
    uint32_t dtc_coolant = lq_j1939_create_dtc(110, J1939_FMI_DATA_ABOVE_NORMAL, 1);
    uint32_t dtc_oil = lq_j1939_create_dtc(100, J1939_FMI_DATA_BELOW_NORMAL, 2);
    
    EXPECT_EQ(lq_j1939_get_spn(dtc_coolant), 110);
    EXPECT_EQ(lq_j1939_get_fmi(dtc_coolant), J1939_FMI_DATA_ABOVE_NORMAL);
    
    EXPECT_EQ(lq_j1939_get_spn(dtc_oil), 100);
    EXPECT_EQ(lq_j1939_get_fmi(dtc_oil), J1939_FMI_DATA_BELOW_NORMAL);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(J1939Test, EndToEndEEC1) {
    // Setup full decode and encode cycle
    signal_ids[0] = 1000;  // RPM
    signal_ids[1] = 1001;  // Torque
    
    decode_maps[0].protocol_id = J1939_PGN_EEC1;
    decode_maps[0].signal_ids = signal_ids;
    decode_maps[0].num_signals = 2;
    
    encode_maps[0].protocol_id = J1939_PGN_EEC1;
    encode_maps[0].period_ms = 50;
    
    config.num_decode_maps = 1;
    config.num_encode_maps = 1;
    
    proto.vtbl->init(&proto, &config);
    
    // 1. Decode incoming message
    uint8_t rx_data[8] = {0xFF, 0xFF, 200, 0, 188, 0xFF, 0xFF, 0xFF};
    uint32_t can_id = make_can_id(6, J1939_PGN_EEC1, 0x10);
    auto rx_msg = make_msg(can_id, rx_data, 8);
    
    struct lq_event events[10];
    size_t num_events = proto.vtbl->decode(&proto, 1000000, &rx_msg, events, 10);
    EXPECT_EQ(num_events, 2);
    
    // 2. Update our own signals
    proto.vtbl->update_signal(&proto, 1000, 1800, 1000000);  // 1800 RPM
    proto.vtbl->update_signal(&proto, 1001, 80, 1000000);    // 80% torque
    
    // 3. Generate cyclic transmission
    struct lq_protocol_msg tx_msgs[10];
    uint8_t tx_buffers[10][8];
    for (int i = 0; i < 10; i++) {
        tx_msgs[i].data = tx_buffers[i];
        tx_msgs[i].capacity = 8;
    }
    
    size_t num_tx = proto.vtbl->get_cyclic(&proto, 50000, tx_msgs, 10);
    EXPECT_EQ(num_tx, 1);
    EXPECT_EQ(lq_j1939_extract_pgn(tx_msgs[0].address), J1939_PGN_EEC1);
}

TEST_F(J1939Test, MultipleProtocolInstances) {
    // Verify multiple independent instances work correctly
    struct lq_j1939_ctx ctx2;
    struct lq_protocol_driver proto2;
    struct lq_protocol_config config2;
    
    memset(&ctx2, 0, sizeof(ctx2));
    memset(&proto2, 0, sizeof(proto2));
    memset(&config2, 0, sizeof(config2));
    
    proto2.vtbl = &lq_j1939_protocol_vtbl;
    proto2.ctx = &ctx2;
    config2.node_address = 0x30;
    
    // Init both instances
    proto.vtbl->init(&proto, &config);
    proto2.vtbl->init(&proto2, &config2);
    
    EXPECT_EQ(ctx.node_address, 0x25);
    EXPECT_EQ(ctx2.node_address, 0x30);
    
    // Update signals independently
    proto.vtbl->update_signal(&proto, 1000, 1500, 0);
    proto2.vtbl->update_signal(&proto2, 1000, 2000, 0);
    
    EXPECT_EQ(ctx.signals[0].value, 1500);
    EXPECT_EQ(ctx2.signals[0].value, 2000);
}

// ============================================================================
// Edge Cases and Robustness
// ============================================================================

TEST_F(J1939Test, DecodeAllPGNs) {
    // Verify all supported PGNs can be decoded
    const uint32_t pgns[] = {J1939_PGN_EEC1, J1939_PGN_ET1};
    
    for (size_t i = 0; i < sizeof(pgns) / sizeof(pgns[0]); i++) {
        signal_ids[0] = 1000 + i;
        decode_maps[0].protocol_id = pgns[i];
        decode_maps[0].signal_ids = signal_ids;
        decode_maps[0].num_signals = 1;
        config.num_decode_maps = 1;
        
        // Reinit with new config
        memset(&ctx, 0, sizeof(ctx));
        proto.vtbl->init(&proto, &config);
        
        uint8_t data[8] = {100, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        uint32_t can_id = make_can_id(6, pgns[i], 0x00);
        auto msg = make_msg(can_id, data, 8);
        
        struct lq_event events[10];
        size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
        
        EXPECT_GT(num, 0) << "PGN " << pgns[i] << " failed to decode";
    }
}

TEST_F(J1939Test, LargeSignalCount) {
    proto.vtbl->init(&proto, &config);
    
    // Add maximum signals
    for (uint32_t i = 0; i < 32; i++) {
        proto.vtbl->update_signal(&proto, i, i * 100, 1000000);
    }
    
    EXPECT_EQ(ctx.num_signals, 32);
    
    // Verify all stored correctly
    for (uint32_t i = 0; i < 32; i++) {
        EXPECT_EQ(ctx.signals[i].signal_id, i);
        EXPECT_EQ(ctx.signals[i].value, i * 100);
    }
}

TEST_F(J1939Test, VtableCompleteness) {
    // Verify all vtable functions are implemented
    EXPECT_NE(lq_j1939_protocol_vtbl.init, nullptr);
    EXPECT_NE(lq_j1939_protocol_vtbl.decode, nullptr);
    EXPECT_NE(lq_j1939_protocol_vtbl.encode, nullptr);
    EXPECT_NE(lq_j1939_protocol_vtbl.get_cyclic, nullptr);
    EXPECT_NE(lq_j1939_protocol_vtbl.update_signal, nullptr);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
