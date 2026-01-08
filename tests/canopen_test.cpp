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
        proto.vtbl->update_signal(&proto, (uint32_t)i, (int32_t)(i * 10), (uint64_t)(i * 1000));
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
        proto.vtbl->update_signal(&proto, i, (int32_t)(i * 100), (uint64_t)(i * 1000));
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

// ============================================================================
// LSS (Layer Setting Services) Tests
// ============================================================================

TEST_F(CANopenTest, LssSetIdentity) {
    proto.vtbl->init(&proto, &config);
    
    int ret = lq_canopen_set_lss_identity(&proto, 0x12345678, 0xABCDEF01, 0x00010002, 0x99887766);
    EXPECT_EQ(ret, 0);
    
    EXPECT_EQ(ctx.vendor_id, 0x12345678);
    EXPECT_EQ(ctx.product_code, 0xABCDEF01);
    EXPECT_EQ(ctx.revision_number, 0x00010002);
    EXPECT_EQ(ctx.serial_number, 0x99887766);
}

TEST_F(CANopenTest, LssSetIdentityNullProto) {
    int ret = lq_canopen_set_lss_identity(nullptr, 0, 0, 0, 0);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssInquireNodeId) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    out_msg.capacity = 8;
    
    int ret = lq_canopen_lss_inquire_node_id(&proto, &out_msg);
    EXPECT_EQ(ret, 0);
    
    // Verify message format
    EXPECT_EQ(out_msg.address, CANOPEN_LSS_MASTER_TX);
    EXPECT_EQ(out_msg.len, 8);
    EXPECT_EQ(out_msg.data[0], CANOPEN_LSS_INQUIRE_NODE_ID);
    
    // Remaining bytes should be zero
    for (int i = 1; i < 8; i++) {
        EXPECT_EQ(out_msg.data[i], 0);
    }
}

TEST_F(CANopenTest, LssInquireNodeIdNullProto) {
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    
    int ret = lq_canopen_lss_inquire_node_id(nullptr, &out_msg);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssInquireNodeIdNullMsg) {
    proto.vtbl->init(&proto, &config);
    
    int ret = lq_canopen_lss_inquire_node_id(&proto, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssConfigureNodeId) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    out_msg.capacity = 8;
    
    // Configure node ID 42
    int ret = lq_canopen_lss_configure_node_id(&proto, 42, &out_msg);
    EXPECT_EQ(ret, 0);
    
    EXPECT_EQ(out_msg.address, CANOPEN_LSS_MASTER_TX);
    EXPECT_EQ(out_msg.len, 8);
    EXPECT_EQ(out_msg.data[0], CANOPEN_LSS_CONFIGURE_NODE_ID);
    EXPECT_EQ(out_msg.data[1], 42);
    
    // Remaining bytes should be zero
    for (int i = 2; i < 8; i++) {
        EXPECT_EQ(out_msg.data[i], 0);
    }
}

TEST_F(CANopenTest, LssConfigureNodeIdUnconfigured) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    out_msg.capacity = 8;
    
    // Configure as unconfigured (255)
    int ret = lq_canopen_lss_configure_node_id(&proto, 255, &out_msg);
    EXPECT_EQ(ret, 0);
    
    EXPECT_EQ(out_msg.data[1], 255);
}

TEST_F(CANopenTest, LssConfigureNodeIdInvalidZero) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    
    // Node ID 0 is invalid
    int ret = lq_canopen_lss_configure_node_id(&proto, 0, &out_msg);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssConfigureNodeIdInvalidRange) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    
    // Node ID 128-254 are invalid (only 1-127 and 255 valid)
    int ret = lq_canopen_lss_configure_node_id(&proto, 128, &out_msg);
    EXPECT_EQ(ret, -1);
    
    ret = lq_canopen_lss_configure_node_id(&proto, 200, &out_msg);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssConfigureNodeIdValidRange) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    out_msg.capacity = 8;
    
    // Test boundary values
    int ret = lq_canopen_lss_configure_node_id(&proto, 1, &out_msg);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out_msg.data[1], 1);
    
    ret = lq_canopen_lss_configure_node_id(&proto, 127, &out_msg);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out_msg.data[1], 127);
}

TEST_F(CANopenTest, LssSwitchStateGlobal) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    out_msg.capacity = 8;
    
    // Switch to configuration state
    int ret = lq_canopen_lss_switch_state_global(&proto, 1, &out_msg);
    EXPECT_EQ(ret, 0);
    
    EXPECT_EQ(out_msg.address, CANOPEN_LSS_MASTER_TX);
    EXPECT_EQ(out_msg.len, 8);
    EXPECT_EQ(out_msg.data[0], CANOPEN_LSS_SWITCH_GLOBAL);
    EXPECT_EQ(out_msg.data[1], 1);
    
    // Switch to waiting state
    ret = lq_canopen_lss_switch_state_global(&proto, 0, &out_msg);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(out_msg.data[1], 0);
}

TEST_F(CANopenTest, LssSwitchStateGlobalNullProto) {
    struct lq_protocol_msg out_msg;
    uint8_t out_data[8];
    out_msg.data = out_data;
    
    int ret = lq_canopen_lss_switch_state_global(nullptr, 1, &out_msg);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, LssReceiveSwitchStateGlobal) {
    proto.vtbl->init(&proto, &config);
    
    // Initial state should be waiting
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_WAITING);
    
    // Receive switch to configuration state
    uint8_t data[8] = {CANOPEN_LSS_SWITCH_GLOBAL, 1, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg = make_msg(CANOPEN_LSS_MASTER_TX, data, 8);
    
    struct lq_event events[8];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
    
    // Should not generate events, but should change state
    EXPECT_EQ(num, 0);
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_CONFIGURATION);
    
    // Switch back to waiting
    data[1] = 0;
    msg = make_msg(CANOPEN_LSS_MASTER_TX, data, 8);
    num = proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
    
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_WAITING);
}

TEST_F(CANopenTest, LssReceiveConfigureNodeId) {
    proto.vtbl->init(&proto, &config);
    
    EXPECT_EQ(ctx.node_id, 0x10);  // Initial node ID
    
    // First switch to configuration state
    uint8_t data1[8] = {CANOPEN_LSS_SWITCH_GLOBAL, 1, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg1 = make_msg(CANOPEN_LSS_MASTER_TX, data1, 8);
    struct lq_event events[8];
    proto.vtbl->decode(&proto, 1000000, &msg1, events, 8);
    
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_CONFIGURATION);
    
    // Now configure new node ID
    uint8_t data2[8] = {CANOPEN_LSS_CONFIGURE_NODE_ID, 50, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg2 = make_msg(CANOPEN_LSS_MASTER_TX, data2, 8);
    proto.vtbl->decode(&proto, 1000000, &msg2, events, 8);
    
    EXPECT_EQ(ctx.node_id, 50);
}

TEST_F(CANopenTest, LssConfigureNodeIdOnlyInConfigState) {
    proto.vtbl->init(&proto, &config);
    
    EXPECT_EQ(ctx.node_id, 0x10);
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_WAITING);
    
    // Try to configure node ID while in waiting state (should be ignored)
    uint8_t data[8] = {CANOPEN_LSS_CONFIGURE_NODE_ID, 99, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg = make_msg(CANOPEN_LSS_MASTER_TX, data, 8);
    struct lq_event events[8];
    proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
    
    // Node ID should not change
    EXPECT_EQ(ctx.node_id, 0x10);
}

TEST_F(CANopenTest, LssInquireMessages) {
    proto.vtbl->init(&proto, &config);
    
    // Set up LSS identity
    lq_canopen_set_lss_identity(&proto, 0x1234, 0x5678, 0xABCD, 0xEF01);
    
    // Switch to configuration state
    uint8_t data1[8] = {CANOPEN_LSS_SWITCH_GLOBAL, 1, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg1 = make_msg(CANOPEN_LSS_MASTER_TX, data1, 8);
    struct lq_event events[8];
    proto.vtbl->decode(&proto, 1000000, &msg1, events, 8);
    
    // Receive various inquire messages (should not crash)
    uint8_t inquire_commands[] = {
        CANOPEN_LSS_INQUIRE_NODE_ID,
        CANOPEN_LSS_INQUIRE_VENDOR_ID,
        CANOPEN_LSS_INQUIRE_PRODUCT_CODE,
        CANOPEN_LSS_INQUIRE_REVISION,
        CANOPEN_LSS_INQUIRE_SERIAL
    };
    
    for (uint8_t cmd : inquire_commands) {
        uint8_t data[8] = {cmd, 0, 0, 0, 0, 0, 0, 0};
        struct lq_protocol_msg msg = make_msg(CANOPEN_LSS_MASTER_TX, data, 8);
        size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
        
        // Should not generate events (responses would be handled separately)
        EXPECT_EQ(num, 0);
    }
}

TEST_F(CANopenTest, LssSlaveResponseMessage) {
    proto.vtbl->init(&proto, &config);
    
    // Receive LSS slave response (master receiving from slave)
    uint8_t data[8] = {CANOPEN_LSS_INQUIRE_NODE_ID, 42, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg = make_msg(CANOPEN_LSS_SLAVE_TX, data, 8);
    
    struct lq_event events[8];
    size_t num = proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
    
    // Should not crash, currently just processes the message
    EXPECT_EQ(num, 0);
}

TEST_F(CANopenTest, LssIdentityAfterInit) {
    proto.vtbl->init(&proto, &config);
    
    // Identity fields should be initialized to zero
    EXPECT_EQ(ctx.vendor_id, 0);
    EXPECT_EQ(ctx.product_code, 0);
    EXPECT_EQ(ctx.revision_number, 0);
    EXPECT_EQ(ctx.serial_number, 0);
    EXPECT_EQ(ctx.lss_state, CANOPEN_LSS_WAITING);
}

TEST_F(CANopenTest, LssConfigureUnconfiguredNodeId) {
    proto.vtbl->init(&proto, &config);
    
    // Switch to configuration state
    uint8_t data1[8] = {CANOPEN_LSS_SWITCH_GLOBAL, 1, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg1 = make_msg(CANOPEN_LSS_MASTER_TX, data1, 8);
    struct lq_event events[8];
    proto.vtbl->decode(&proto, 1000000, &msg1, events, 8);
    
    // Configure as unconfigured (255)
    uint8_t data2[8] = {CANOPEN_LSS_CONFIGURE_NODE_ID, 255, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg2 = make_msg(CANOPEN_LSS_MASTER_TX, data2, 8);
    proto.vtbl->decode(&proto, 1000000, &msg2, events, 8);
    
    EXPECT_EQ(ctx.node_id, 255);
}

TEST_F(CANopenTest, LssConfigureInvalidNodeId) {
    proto.vtbl->init(&proto, &config);
    
    // Switch to configuration state
    uint8_t data1[8] = {CANOPEN_LSS_SWITCH_GLOBAL, 1, 0, 0, 0, 0, 0, 0};
    struct lq_protocol_msg msg1 = make_msg(CANOPEN_LSS_MASTER_TX, data1, 8);
    struct lq_event events[8];
    proto.vtbl->decode(&proto, 1000000, &msg1, events, 8);
    
    // Try invalid node IDs (0, 128-254)
    uint8_t invalid_ids[] = {0, 128, 150, 200, 254};
    
    for (uint8_t id : invalid_ids) {
        uint8_t original_id = ctx.node_id;
        uint8_t data[8] = {CANOPEN_LSS_CONFIGURE_NODE_ID, id, 0, 0, 0, 0, 0, 0};
        struct lq_protocol_msg msg = make_msg(CANOPEN_LSS_MASTER_TX, data, 8);
        proto.vtbl->decode(&proto, 1000000, &msg, events, 8);
        
        // Node ID should not change for invalid values
        EXPECT_EQ(ctx.node_id, original_id);
    }
}

// ============================================================================
// Protocol Create Function Tests
// ============================================================================

TEST_F(CANopenTest, ProtocolCreateBasic) {
    struct lq_canopen_ctx new_ctx;
    struct lq_protocol_driver new_proto;
    
    int ret = lq_canopen_protocol_create(&new_proto, &new_ctx, &config);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(new_proto.vtbl, &lq_canopen_protocol_vtbl);
    EXPECT_EQ(new_proto.ctx, &new_ctx);
    EXPECT_EQ(((struct lq_canopen_ctx*)new_proto.ctx)->node_id, 0x10);
}

TEST_F(CANopenTest, ProtocolCreateNullProto) {
    struct lq_canopen_ctx new_ctx;
    int ret = lq_canopen_protocol_create(nullptr, &new_ctx, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ProtocolCreateNullContext) {
    struct lq_protocol_driver new_proto;
    int ret = lq_canopen_protocol_create(&new_proto, nullptr, &config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ProtocolCreateNullConfig) {
    struct lq_canopen_ctx new_ctx;
    struct lq_protocol_driver new_proto;
    int ret = lq_canopen_protocol_create(&new_proto, &new_ctx, nullptr);
    EXPECT_EQ(ret, -1);
}

// ============================================================================
// Additional Error Handling Tests
// ============================================================================

TEST_F(CANopenTest, SetNmtStateNullProto) {
    lq_canopen_set_nmt_state(nullptr, CANOPEN_STATE_OPERATIONAL);
    // Should not crash
}

TEST_F(CANopenTest, SetNmtStateNullContext) {
    proto.ctx = nullptr;
    lq_canopen_set_nmt_state(&proto, CANOPEN_STATE_OPERATIONAL);
    // Should not crash
}

TEST_F(CANopenTest, SendEmergencyNullProto) {
    lq_canopen_send_emergency(nullptr, 0x1000, 0x01, nullptr);
    // Should not crash
}

TEST_F(CANopenTest, SendEmergencyNullContext) {
    proto.ctx = nullptr;
    lq_canopen_send_emergency(&proto, 0x1000, 0x01, nullptr);
    // Should not crash
}

TEST_F(CANopenTest, ConfigureTpdoInvalidPdoNum) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_canopen_pdo_config pdo_config;
    memset(&pdo_config, 0, sizeof(pdo_config));
    
    // PDO number 0 (invalid)
    int ret = lq_canopen_configure_tpdo(&proto, 0, &pdo_config);
    EXPECT_EQ(ret, -1);
    
    // PDO number 5 (invalid)
    ret = lq_canopen_configure_tpdo(&proto, 5, &pdo_config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ConfigureTpdoNullProto) {
    struct lq_canopen_pdo_config pdo_config = {};
    int ret = lq_canopen_configure_tpdo(nullptr, 1, &pdo_config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ConfigureTpdoNullConfig) {
    proto.vtbl->init(&proto, &config);
    int ret = lq_canopen_configure_tpdo(&proto, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ConfigureRpdoInvalidPdoNum) {
    proto.vtbl->init(&proto, &config);
    
    struct lq_canopen_pdo_config pdo_config;
    memset(&pdo_config, 0, sizeof(pdo_config));
    
    int ret = lq_canopen_configure_rpdo(&proto, 0, &pdo_config);
    EXPECT_EQ(ret, -1);
    
    ret = lq_canopen_configure_rpdo(&proto, 5, &pdo_config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ConfigureRpdoNullProto) {
    struct lq_canopen_pdo_config pdo_config = {};
    int ret = lq_canopen_configure_rpdo(nullptr, 1, &pdo_config);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, ConfigureRpdoNullConfig) {
    proto.vtbl->init(&proto, &config);
    int ret = lq_canopen_configure_rpdo(&proto, 1, nullptr);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, SetLssIdentityNullProto) {
    int ret = lq_canopen_set_lss_identity(nullptr, 1, 2, 3, 4);
    EXPECT_EQ(ret, -1);
}

TEST_F(CANopenTest, SetLssIdentityNullContext) {
    proto.ctx = nullptr;
    int ret = lq_canopen_set_lss_identity(&proto, 1, 2, 3, 4);
    EXPECT_EQ(ret, -1);
}
