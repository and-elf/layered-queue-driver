/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Google Test tests for CANopen Protocol Driver
 * Comprehensive coverage for production validation
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <algorithm>

extern "C" {
#include "lq_canopen.h"
#include "lq_protocol.h"
}

// ============================================================================
// Test Fixture
// ============================================================================

class CANopenTest : public ::testing::Test {
protected:
    struct lq_canopen_ctx ctx;
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
        proto.vtbl = &lq_canopen_protocol_vtbl;
        proto.ctx = &ctx;
        
        // Basic config
        config.node_address = 0x10;  // Node ID 16
        config.decode_maps = decode_maps;
        config.encode_maps = encode_maps;
        config.num_decode_maps = 0;
        config.num_encode_maps = 0;
    }
    
    void TearDown() override {
        // No dynamic allocation, nothing to free
    }
    
    // Helper to create protocol message
    struct lq_protocol_msg make_msg(uint16_t cob_id, const uint8_t* data, size_t len) {
        struct lq_protocol_msg msg;
        msg.address = cob_id;
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

TEST_F(CANopenTest, InitBasic) {
    int ret = proto.vtbl->init(&proto, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(ctx.node_id, 0x10);
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_PRE_OPERATIONAL);
    EXPECT_EQ(ctx.num_signals, 0);
}

TEST_F(CANopenTest, InitNullProtocol) {
    int ret = proto.vtbl->init(nullptr, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, InitNullConfig) {
    int ret = proto.vtbl->init(&proto, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, InitDefaultHeartbeat) {
    proto.vtbl->init(&proto, &config);
    EXPECT_EQ(ctx.heartbeat_period_ms, 1000);  // Default 1s
}

// ============================================================================
// COB-ID Helper Tests
// ============================================================================

TEST_F(CANopenTest, BuildCobId) {
    uint16_t cob_id = lq_canopen_build_cob_id(CANOPEN_FC_TPDO1, 0x10);
    EXPECT_EQ(cob_id, 0x190);  // 0x180 + 0x10
}

TEST_F(CANopenTest, GetNodeIdFromCobId) {
    uint8_t node_id = lq_canopen_get_node_id(0x190, CANOPEN_FC_TPDO1);
    EXPECT_EQ(node_id, 0x10);
}

TEST_F(CANopenTest, FunctionCodeConstants) {
    EXPECT_EQ(CANOPEN_FC_NMT, 0x000);
    EXPECT_EQ(CANOPEN_FC_SYNC, 0x080);
    EXPECT_EQ(CANOPEN_FC_TPDO1, 0x180);
    EXPECT_EQ(CANOPEN_FC_RPDO1, 0x200);
    EXPECT_EQ(CANOPEN_FC_HEARTBEAT, 0x700);
}

// ============================================================================
// NMT Tests
// ============================================================================

TEST_F(CANopenTest, NmtStartCommand) {
    proto.vtbl->init(&proto, &config);
    
    // Send NMT Start command
    uint8_t data[2] = {CANOPEN_NMT_START, 0x10};  // Start node 0x10
    auto msg = make_msg(CANOPEN_FC_NMT, data, 2);
    
    struct lq_event events[10];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_OPERATIONAL);
}

TEST_F(CANopenTest, NmtStopCommand) {
    proto.vtbl->init(&proto, &config);
    
    uint8_t data[2] = {CANOPEN_NMT_STOP, 0x10};
    auto msg = make_msg(CANOPEN_FC_NMT, data, 2);
    
    struct lq_event events[10];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_STOPPED);
}

TEST_F(CANopenTest, NmtPreOperationalCommand) {
    proto.vtbl->init(&proto, &config);
    ctx.nmt_state = CANOPEN_STATE_OPERATIONAL;
    
    uint8_t data[2] = {CANOPEN_NMT_PRE_OPERATIONAL, 0x10};
    auto msg = make_msg(CANOPEN_FC_NMT, data, 2);
    
    struct lq_event events[10];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_PRE_OPERATIONAL);
}

TEST_F(CANopenTest, NmtBroadcastCommand) {
    proto.vtbl->init(&proto, &config);
    
    // Broadcast to all nodes (node_id = 0)
    uint8_t data[2] = {CANOPEN_NMT_START, 0x00};
    auto msg = make_msg(CANOPEN_FC_NMT, data, 2);
    
    struct lq_event events[10];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_OPERATIONAL);
}

TEST_F(CANopenTest, NmtIgnoreDifferentNode) {
    proto.vtbl->init(&proto, &config);
    
    // Command for node 0x20, not 0x10
    uint8_t data[2] = {CANOPEN_NMT_START, 0x20};
    auto msg = make_msg(CANOPEN_FC_NMT, data, 2);
    
    struct lq_event events[10];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_PRE_OPERATIONAL);  // Unchanged
}

// ============================================================================
// SYNC Tests
// ============================================================================

TEST_F(CANopenTest, SyncCounterIncrement) {
    proto.vtbl->init(&proto, &config);
    
    uint8_t data[1] = {0};
    auto msg = make_msg(CANOPEN_FC_SYNC, data, 0);
    
    struct lq_event events[10];
    
    EXPECT_EQ(ctx.sync_counter, 0);
    proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    EXPECT_EQ(ctx.sync_counter, 1);
    proto.vtbl->decode(&proto, 2000000, &msg, events, 10);
    EXPECT_EQ(ctx.sync_counter, 2);
}

// ============================================================================
// PDO Decode Tests (RPDO)
// ============================================================================

TEST_F(CANopenTest, DecodeRpdoBasic) {
    proto.vtbl->init(&proto, &config);
    
    // Configure RPDO1 with 2 signals
    ctx.rpdo[0].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10);
    ctx.rpdo[0].num_mappings = 2;
    ctx.rpdo[0].mappings[0].signal_id = 100;
    ctx.rpdo[0].mappings[0].length = 16;
    ctx.rpdo[0].mappings[1].signal_id = 101;
    ctx.rpdo[0].mappings[1].length = 16;
    
    // Create PDO message: 16-bit values 1234 and 5678
    uint8_t data[8] = {0xD2, 0x04, 0x2E, 0x16, 0, 0, 0, 0};  // 1234, 5678 little-endian
    auto msg = make_msg(ctx.rpdo[0].cob_id, data, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num, 2);
    EXPECT_EQ(events[0].source_id, 100);
    EXPECT_EQ(events[0].value, 1234);
    EXPECT_EQ(events[1].source_id, 101);
    EXPECT_EQ(events[1].value, 5678);
}

TEST_F(CANopenTest, DecodeRpdo8Bit) {
    proto.vtbl->init(&proto, &config);
    
    ctx.rpdo[0].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10);
    ctx.rpdo[0].num_mappings = 1;
    ctx.rpdo[0].mappings[0].signal_id = 200;
    ctx.rpdo[0].mappings[0].length = 8;
    
    uint8_t data[8] = {42, 0, 0, 0, 0, 0, 0, 0};
    auto msg = make_msg(ctx.rpdo[0].cob_id, data, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num, 1);
    EXPECT_EQ(events[0].source_id, 200);
    EXPECT_EQ(events[0].value, 42);
}

TEST_F(CANopenTest, DecodeRpdo32Bit) {
    proto.vtbl->init(&proto, &config);
    
    ctx.rpdo[0].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10);
    ctx.rpdo[0].num_mappings = 1;
    ctx.rpdo[0].mappings[0].signal_id = 300;
    ctx.rpdo[0].mappings[0].length = 32;
    
    // 0x12345678 in little-endian
    uint8_t data[8] = {0x78, 0x56, 0x34, 0x12, 0, 0, 0, 0};
    auto msg = make_msg(ctx.rpdo[0].cob_id, data, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num, 1);
    EXPECT_EQ(events[0].source_id, 300);
    EXPECT_EQ(events[0].value, 0x12345678);
}

TEST_F(CANopenTest, DecodeRpdoMultiplePdos) {
    proto.vtbl->init(&proto, &config);
    
    // Configure RPDO1 and RPDO2
    ctx.rpdo[0].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10);
    ctx.rpdo[0].num_mappings = 1;
    ctx.rpdo[0].mappings[0].signal_id = 100;
    ctx.rpdo[0].mappings[0].length = 16;
    
    ctx.rpdo[1].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO2, 0x10);
    ctx.rpdo[1].num_mappings = 1;
    ctx.rpdo[1].mappings[0].signal_id = 101;
    ctx.rpdo[1].mappings[0].length = 16;
    
    // Decode RPDO1
    uint8_t data1[8] = {100, 0, 0, 0, 0, 0, 0, 0};
    auto msg1 = make_msg(ctx.rpdo[0].cob_id, data1, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg1, events, 10);
    EXPECT_EQ(num, 1);
    EXPECT_EQ(events[0].value, 100);
    
    // Decode RPDO2
    uint8_t data2[8] = {200, 0, 0, 0, 0, 0, 0, 0};
    auto msg2 = make_msg(ctx.rpdo[1].cob_id, data2, 8);
    num = proto.vtbl->decode(&proto, 2000000, &msg2, events, 10);
    EXPECT_EQ(num, 1);
    EXPECT_EQ(events[0].value, 200);
}

TEST_F(CANopenTest, DecodeRpdoUnmapped) {
    proto.vtbl->init(&proto, &config);
    
    // No RPDOs configured
    uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    auto msg = make_msg(lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10), data, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    EXPECT_EQ(num, 0);  // No events generated
}

// ============================================================================
// Decode Error Handling Tests
// ============================================================================

TEST_F(CANopenTest, DecodeNullProtocol) {
    uint8_t data[8] = {0};
    auto msg = make_msg(0x100, data, 8);
    struct lq_event events[10];
    
    size_t num = proto.vtbl->decode(nullptr, 1000000, &msg, events, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, DecodeNullMessage) {
    proto.vtbl->init(&proto, &config);
    struct lq_event events[10];
    
    size_t num = proto.vtbl->decode(&proto, 1000000, nullptr, events, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, DecodeNullEvents) {
    proto.vtbl->init(&proto, &config);
    uint8_t data[8] = {0};
    auto msg = make_msg(0x100, data, 8);
    
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, nullptr, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, DecodeMaxEventsZero) {
    proto.vtbl->init(&proto, &config);
    uint8_t data[8] = {0};
    auto msg = make_msg(0x100, data, 8);
    struct lq_event events[10];
    
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 0);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, DecodeNullDataPointer) {
    proto.vtbl->init(&proto, &config);
    struct lq_protocol_msg msg;
    msg.address = 0x100;
    msg.data = nullptr;
    msg.len = 8;
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    EXPECT_EQ(num, 0);
}

// ============================================================================
// Signal Update Tests
// ============================================================================

TEST_F(CANopenTest, UpdateSignalBasic) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 1000, 1234, 1000000);
    
    EXPECT_EQ(ctx.num_signals, 1);
    EXPECT_EQ(ctx.signals[0].signal_id, 1000);
    EXPECT_EQ(ctx.signals[0].value, 1234);
    EXPECT_EQ(ctx.signals[0].timestamp, 1000000);
}

TEST_F(CANopenTest, UpdateSignalMultiple) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 100, 10, 1000);
    proto.vtbl->update_signal(&proto, 101, 20, 2000);
    proto.vtbl->update_signal(&proto, 102, 30, 3000);
    
    EXPECT_EQ(ctx.num_signals, 3);
    EXPECT_EQ(ctx.signals[0].value, 10);
    EXPECT_EQ(ctx.signals[1].value, 20);
    EXPECT_EQ(ctx.signals[2].value, 30);
}

TEST_F(CANopenTest, UpdateSignalOverwrite) {
    proto.vtbl->init(&proto, &config);
    
    proto.vtbl->update_signal(&proto, 100, 10, 1000);
    proto.vtbl->update_signal(&proto, 100, 20, 2000);
    
    // Should overwrite, not add new
    EXPECT_EQ(ctx.num_signals, 1);
    EXPECT_EQ(ctx.signals[0].value, 20);
    EXPECT_EQ(ctx.signals[0].timestamp, 2000);
}

TEST_F(CANopenTest, UpdateSignalMaxCapacity) {
    proto.vtbl->init(&proto, &config);
    
    // Fill to capacity (64 signals)
    for (int i = 0; i < 64; i++) {
        proto.vtbl->update_signal(&proto, i, i * 10, i * 1000);
    }
    
    EXPECT_EQ(ctx.num_signals, 64);
    
    // Try to add one more - should be ignored
    proto.vtbl->update_signal(&proto, 100, 999, 100000);
    EXPECT_EQ(ctx.num_signals, 64);
}

TEST_F(CANopenTest, UpdateSignalNullProtocol) {
    proto.vtbl->update_signal(nullptr, 100, 10, 1000);
    // Should not crash
}

// ============================================================================
// Heartbeat Tests
// ============================================================================

TEST_F(CANopenTest, HeartbeatInPreOperational) {
    proto.vtbl->init(&proto, &config);
    ctx.nmt_state = CANOPEN_STATE_PRE_OPERATIONAL;
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // In pre-operational, no cyclic messages
    size_t num = proto.vtbl->get_cyclic(&proto, 0, msgs, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, HeartbeatInOperational) {
    proto.vtbl->init(&proto, &config);
    ctx.nmt_state = CANOPEN_STATE_OPERATIONAL;
    ctx.heartbeat_period_ms = 1000;
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // First heartbeat at 1000ms
    size_t num = proto.vtbl->get_cyclic(&proto, 1000000, msgs, 10);
    EXPECT_EQ(num, 1);
    EXPECT_EQ(msgs[0].address, lq_canopen_build_cob_id(CANOPEN_FC_HEARTBEAT, 0x10));
    EXPECT_EQ(msgs[0].len, 1);
    EXPECT_EQ(msgs[0].data[0], CANOPEN_STATE_OPERATIONAL);
}

TEST_F(CANopenTest, HeartbeatTiming) {
    proto.vtbl->init(&proto, &config);
    ctx.nmt_state = CANOPEN_STATE_OPERATIONAL;
    ctx.heartbeat_period_ms = 500;
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    // First heartbeat at 500ms
    size_t num = proto.vtbl->get_cyclic(&proto, 500000, msgs, 10);
    EXPECT_EQ(num, 1);
    
    // Too early at 800ms
    num = proto.vtbl->get_cyclic(&proto, 800000, msgs, 10);
    EXPECT_EQ(num, 0);
    
    // Second heartbeat at 1000ms
    num = proto.vtbl->get_cyclic(&proto, 1000000, msgs, 10);
    EXPECT_EQ(num, 1);
}

// ============================================================================
// Emergency Tests
// ============================================================================

TEST_F(CANopenTest, EmergencySend) {
    proto.vtbl->init(&proto, &config);
    ctx.nmt_state = CANOPEN_STATE_OPERATIONAL;
    ctx.emcy_pending = true;
    ctx.emcy_error_code = CANOPEN_EMCY_TEMPERATURE;
    ctx.last_heartbeat_time = 100000;  // Set recent heartbeat to avoid heartbeat transmission
    
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    size_t num = proto.vtbl->get_cyclic(&proto, 200000, msgs, 10);  // Not enough time for heartbeat
    
    // Should send emergency (before heartbeat)
    EXPECT_GE(num, 1);
    
    // Find the emergency message (should be first or only message)
    bool found_emcy = false;
    for (size_t i = 0; i < num; i++) {
        if ((msgs[i].address & 0x780) == CANOPEN_FC_EMCY) {
            found_emcy = true;
            EXPECT_EQ(msgs[i].address, lq_canopen_build_cob_id(CANOPEN_FC_EMCY, 0x10));
            EXPECT_EQ(msgs[i].len, 8);
            EXPECT_EQ(msgs[i].data[0], (CANOPEN_EMCY_TEMPERATURE & 0xFF));
            EXPECT_EQ(msgs[i].data[1], ((CANOPEN_EMCY_TEMPERATURE >> 8) & 0xFF));
            break;
        }
    }
    EXPECT_TRUE(found_emcy);
    
    // Emergency should be cleared
    EXPECT_FALSE(ctx.emcy_pending);
}

// ============================================================================
// Cyclic Tests
// ============================================================================

TEST_F(CANopenTest, GetCyclicNullProtocol) {
    struct lq_protocol_msg msgs[10];
    uint8_t buffers[10][8];
    for (int i = 0; i < 10; i++) {
        msgs[i].data = buffers[i];
        msgs[i].capacity = 8;
    }
    
    size_t num = proto.vtbl->get_cyclic(nullptr, 0, msgs, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, GetCyclicNullMessages) {
    proto.vtbl->init(&proto, &config);
    size_t num = proto.vtbl->get_cyclic(&proto, 0, nullptr, 10);
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, GetCyclicMaxMessagesZero) {
    proto.vtbl->init(&proto, &config);
    struct lq_protocol_msg msgs[10];
    size_t num = proto.vtbl->get_cyclic(&proto, 0, msgs, 0);
    EXPECT_EQ(num, 0);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(CANopenTest, EndToEndRpdo) {
    proto.vtbl->init(&proto, &config);
    
    // 1. Configure RPDO
    ctx.rpdo[0].cob_id = lq_canopen_build_cob_id(CANOPEN_FC_RPDO1, 0x10);
    ctx.rpdo[0].num_mappings = 2;
    ctx.rpdo[0].mappings[0].signal_id = 1000;
    ctx.rpdo[0].mappings[0].length = 16;
    ctx.rpdo[0].mappings[1].signal_id = 1001;
    ctx.rpdo[0].mappings[1].length = 16;
    
    // 2. Receive PDO
    uint8_t data[8] = {0x10, 0x27, 0x20, 0x4E, 0, 0, 0, 0};  // 10000, 20000
    auto msg = make_msg(ctx.rpdo[0].cob_id, data, 8);
    
    struct lq_event events[10];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 10);
    
    // 3. Verify events
    EXPECT_EQ(num, 2);
    EXPECT_EQ(events[0].source_id, 1000);
    EXPECT_EQ(events[0].value, 10000);
    EXPECT_EQ(events[1].source_id, 1001);
    EXPECT_EQ(events[1].value, 20000);
}

TEST_F(CANopenTest, MultipleProtocolInstances) {
    // Verify multiple independent instances work correctly
    struct lq_canopen_ctx ctx2;
    struct lq_protocol_driver proto2;
    struct lq_protocol_config config2;
    
    memset(&ctx2, 0, sizeof(ctx2));
    memset(&proto2, 0, sizeof(proto2));
    memset(&config2, 0, sizeof(config2));
    
    proto2.vtbl = &lq_canopen_protocol_vtbl;
    proto2.ctx = &ctx2;
    config2.node_address = 0x20;
    
    // Init both instances
    proto.vtbl->init(&proto, &config);
    proto2.vtbl->init(&proto2, &config2);
    
    EXPECT_EQ(ctx.node_id, 0x10);
    EXPECT_EQ(ctx2.node_id, 0x20);
    
    // Update signals independently
    proto.vtbl->update_signal(&proto, 1000, 100, 0);
    proto2.vtbl->update_signal(&proto2, 1000, 200, 0);
    
    EXPECT_EQ(ctx.signals[0].value, 100);
    EXPECT_EQ(ctx2.signals[0].value, 200);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(CANopenTest, LargeSignalCount) {
    proto.vtbl->init(&proto, &config);
    
    // Update maximum number of signals (64)
    for (uint32_t i = 0; i < 64; i++) {
        proto.vtbl->update_signal(&proto, i, i * 100, i * 1000);
    }
    
    EXPECT_EQ(ctx.num_signals, 64);
    
    // Verify all values
    for (uint32_t i = 0; i < 64; i++) {
        EXPECT_EQ(ctx.signals[i].signal_id, i);
        EXPECT_EQ(ctx.signals[i].value, i * 100);
    }
}

TEST_F(CANopenTest, VtableCompleteness) {
    // Verify all vtable functions are implemented
    EXPECT_NE(lq_canopen_protocol_vtbl.init, nullptr);
    EXPECT_NE(lq_canopen_protocol_vtbl.decode, nullptr);
    EXPECT_NE(lq_canopen_protocol_vtbl.encode, nullptr);
    EXPECT_NE(lq_canopen_protocol_vtbl.get_cyclic, nullptr);
    EXPECT_NE(lq_canopen_protocol_vtbl.update_signal, nullptr);
}

TEST_F(CANopenTest, NmtStateTransitions) {
    proto.vtbl->init(&proto, &config);
    
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_PRE_OPERATIONAL);
    
    lq_canopen_set_nmt_state(&proto, CANOPEN_STATE_OPERATIONAL);
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_OPERATIONAL);
    
    lq_canopen_set_nmt_state(&proto, CANOPEN_STATE_STOPPED);
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_STOPPED);
    
    lq_canopen_set_nmt_state(&proto, CANOPEN_STATE_PRE_OPERATIONAL);
    EXPECT_EQ(ctx.nmt_state, CANOPEN_STATE_PRE_OPERATIONAL);
}
