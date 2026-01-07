/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * ISO-TP (ISO 15765-2) Tests
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>

extern "C" {
#include "lq_isotp.h"
#include "lq_uds_can.h"
}

/* ============================================================================
 * Mock CAN Layer
 * ============================================================================ */

static std::vector<lq_isotp_can_frame> sent_frames;

static int mock_can_send(void *ctx, const struct lq_isotp_can_frame *frame)
{
    (void)ctx;
    sent_frames.push_back(*frame);
    return 0;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class ISOTPTest : public ::testing::Test {
protected:
    struct lq_isotp_channel channel;
    struct lq_isotp_config config;
    uint8_t rx_buffer[4095];
    
    void SetUp() override {
        sent_frames.clear();
        
        memset(&config, 0, sizeof(config));
        config.tx_id = 0x7E0;
        config.rx_id = 0x7E8;
        config.use_extended_id = false;
        config.n_bs = 0;  /* Unlimited block size */
        config.n_st_min = 10;  /* 10ms between CF */
        config.tx_buf_size = 4095;
        config.rx_buf_size = 4095;
        
        ASSERT_EQ(lq_isotp_init(&channel, &config, rx_buffer), 0);
    }
};

/* ============================================================================
 * Single Frame Tests
 * ============================================================================ */

TEST_F(ISOTPTest, SendSingleFrame_1Byte) {
    uint8_t data[] = {0x3E};
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    EXPECT_EQ(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].can_id, 0x7E0);
    EXPECT_EQ(sent_frames[0].dlc, 2);
    EXPECT_EQ(sent_frames[0].data[0], 0x01);  /* SF, len=1 */
    EXPECT_EQ(sent_frames[0].data[1], 0x3E);
    
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

TEST_F(ISOTPTest, SendSingleFrame_7Bytes) {
    uint8_t data[] = {0x22, 0xF1, 0x90, 0x01, 0x02, 0x03, 0x04};
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    EXPECT_EQ(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].dlc, 8);
    EXPECT_EQ(sent_frames[0].data[0], 0x07);  /* SF, len=7 */
    EXPECT_EQ(memcmp(&sent_frames[0].data[1], data, 7), 0);
}

TEST_F(ISOTPTest, ReceiveSingleFrame) {
    struct lq_isotp_can_frame frame = {
        .can_id = 0x7E8,
        .is_extended = false,
        .dlc = 3,
        .data = {0x02, 0x50, 0x01}  /* SF len=2: "50 01" */
    };
    
    ASSERT_EQ(lq_isotp_recv(&channel, &frame, mock_can_send, nullptr, 0), 0);
    
    const uint8_t *rx_data;
    size_t rx_len;
    EXPECT_TRUE(lq_isotp_rx_available(&channel, &rx_data, &rx_len));
    EXPECT_EQ(rx_len, 2);
    EXPECT_EQ(rx_data[0], 0x50);
    EXPECT_EQ(rx_data[1], 0x01);
}

/* ============================================================================
 * Multi-Frame Tests
 * ============================================================================ */

TEST_F(ISOTPTest, SendMultiFrame_8Bytes) {
    uint8_t data[8] = {0x22, 0xF1, 0x90, 0x01, 0x02, 0x03, 0x04, 0x05};
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* Should send FF */
    ASSERT_EQ(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].data[0], 0x10);  /* FF, len high nibble */
    EXPECT_EQ(sent_frames[0].data[1], 0x08);  /* len=8 */
    EXPECT_EQ(memcmp(&sent_frames[0].data[2], data, 6), 0);  /* First 6 bytes */
    
    EXPECT_FALSE(lq_isotp_tx_done(&channel));
    
    /* Simulate FC CTS */
    struct lq_isotp_can_frame fc = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x30, 0x00, 0x0A}  /* FC: CTS, BS=0, STmin=10ms */
    };
    
    ASSERT_EQ(lq_isotp_recv(&channel, &fc, mock_can_send, nullptr, 10000), 0);
    
    /* Should send CF with remaining 2 bytes */
    ASSERT_EQ(lq_isotp_periodic(&channel, mock_can_send, nullptr, 20000), 0);
    
    ASSERT_EQ(sent_frames.size(), 2);
    EXPECT_EQ(sent_frames[1].data[0], 0x21);  /* CF, SN=1 */
    EXPECT_EQ(sent_frames[1].data[1], 0x04);
    EXPECT_EQ(sent_frames[1].data[2], 0x05);
    
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

TEST_F(ISOTPTest, SendMultiFrame_Large) {
    uint8_t data[100];
    for (int i = 0; i < 100; i++) {
        data[i] = (uint8_t)i;
    }
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* FF sent */
    ASSERT_EQ(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].data[0], 0x10);
    EXPECT_EQ(sent_frames[0].data[1], 100);
    
    /* Send FC */
    struct lq_isotp_can_frame fc = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x30, 0x00, 0x00}  /* CTS, BS=0, STmin=0 */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc, mock_can_send, nullptr, 0), 0);
    
    /* Send all CFs */
    for (int i = 0; i < 20; i++) {
        ASSERT_EQ(lq_isotp_periodic(&channel, mock_can_send, nullptr, i * 1000), 0);
        if (lq_isotp_tx_done(&channel)) break;
    }
    
    /* FF + 14 CF (6 + 14*7 = 104 bytes capacity for 100 bytes) */
    EXPECT_EQ(sent_frames.size(), 15);
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

TEST_F(ISOTPTest, ReceiveMultiFrame) {
    /* FF: 20 bytes */
    struct lq_isotp_can_frame ff = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x10, 0x14, 0x62, 0xF1, 0x90, 0x31, 0x48, 0x47}  /* len=20 */
    };
    
    ASSERT_EQ(lq_isotp_recv(&channel, &ff, mock_can_send, nullptr, 0), 0);
    
    /* Should send FC */
    ASSERT_EQ(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].data[0], 0x30);  /* FC CTS */
    
    const uint8_t *rx_data;
    size_t rx_len;
    EXPECT_FALSE(lq_isotp_rx_available(&channel, &rx_data, &rx_len));  /* Not complete yet */
    
    /* CF1: next 7 bytes */
    struct lq_isotp_can_frame cf1 = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x21, 0x42, 0x48, 0x34, 0x31, 0x4A, 0x58, 0x4D}
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &cf1, mock_can_send, nullptr, 10000), 0);
    
    /* CF2: final 7 bytes */
    struct lq_isotp_can_frame cf2 = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x22, 0x4E, 0x31, 0x30, 0x39, 0x31, 0x38, 0x36}
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &cf2, mock_can_send, nullptr, 20000), 0);
    
    /* Now should be complete */
    EXPECT_TRUE(lq_isotp_rx_available(&channel, &rx_data, &rx_len));
    EXPECT_EQ(rx_len, 20);
    EXPECT_EQ(rx_data[0], 0x62);
    EXPECT_EQ(rx_data[1], 0xF1);
    EXPECT_EQ(rx_data[2], 0x90);
}

/* ============================================================================
 * Flow Control Tests
 * ============================================================================ */

TEST_F(ISOTPTest, FlowControl_BlockSize) {
    uint8_t data[50];
    memset(data, 0xAA, sizeof(data));
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* FF sent */
    EXPECT_EQ(sent_frames.size(), 1);
    
    /* FC with BS=3 (send 3 CFs then wait) */
    struct lq_isotp_can_frame fc = {
        .can_id = 0x7E8,
        .is_extended = false,
        .dlc = 3,
        .data = {0x30, 0x03, 0x00}  /* CTS, BS=3, STmin=0 */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc, mock_can_send, nullptr, 0), 0);
    
    /* Send first block (3 CFs) */
    for (int i = 0; i < 5; i++) {
        lq_isotp_periodic(&channel, mock_can_send, nullptr, i * 1000);
    }
    
    /* Should have FF + 3 CFs */
    EXPECT_EQ(sent_frames.size(), 4);
    EXPECT_FALSE(lq_isotp_tx_done(&channel));  /* Waiting for next FC */
    
    /* Send multiple FCs and continue until done */
    for (int block = 0; block < 5 && !lq_isotp_tx_done(&channel); block++) {
        /* Send FC */
        ASSERT_EQ(lq_isotp_recv(&channel, &fc, mock_can_send, nullptr, (5 + block * 10) * 1000), 0);
        
        /* Send next block */
        for (int i = 0; i < 5; i++) {
            lq_isotp_periodic(&channel, mock_can_send, nullptr, (5 + block * 10 + i) * 1000);
        }
    }
    
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

TEST_F(ISOTPTest, FlowControl_Wait) {
    uint8_t data[20];
    memset(data, 0xBB, sizeof(data));
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* FC WAIT */
    struct lq_isotp_can_frame fc_wait = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x31, 0x00, 0x00}  /* FC WAIT */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc_wait, mock_can_send, nullptr, 0), 0);
    
    /* Should not send CFs yet */
    lq_isotp_periodic(&channel, mock_can_send, nullptr, 10000);
    EXPECT_EQ(sent_frames.size(), 1);  /* Only FF */
    
    /* Send FC CTS */
    struct lq_isotp_can_frame fc_cts = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x30, 0x00, 0x00}  /* FC CTS */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc_cts, mock_can_send, nullptr, 20000), 0);
    
    /* Now should send CFs */
    for (int i = 0; i < 5; i++) {
        lq_isotp_periodic(&channel, mock_can_send, nullptr, (20 + i) * 1000);
    }
    
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

TEST_F(ISOTPTest, FlowControl_Overflow) {
    uint8_t data[20];
    memset(data, 0xCC, sizeof(data));
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* FC OVERFLOW - abort */
    struct lq_isotp_can_frame fc_ovf = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x32, 0x00, 0x00}  /* FC OVERFLOW */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc_ovf, mock_can_send, nullptr, 0), 0);
    
    /* Should abort */
    EXPECT_TRUE(lq_isotp_tx_done(&channel));
}

/* ============================================================================
 * Timing Tests
 * ============================================================================ */

TEST_F(ISOTPTest, STmin_Timing) {
    config.n_st_min = 20;  /* 20ms minimum separation */
    ASSERT_EQ(lq_isotp_init(&channel, &config, rx_buffer), 0);
    
    uint8_t data[30];
    memset(data, 0xDD, sizeof(data));
    
    ASSERT_EQ(lq_isotp_send(&channel, data, sizeof(data), mock_can_send, nullptr, 0), 0);
    
    /* FC with STmin=20ms */
    struct lq_isotp_can_frame fc = {
        .can_id = 0x7E8,
        .dlc = 3,
        .data = {0x30, 0x00, 0x14}  /* CTS, BS=0, STmin=20ms */
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &fc, mock_can_send, nullptr, 0), 0);
    
    /* Try to send too quickly - should only send when time elapsed */
    size_t prev_count = sent_frames.size();
    
    for (uint64_t t = 0; t < 100000; t += 5000) {  /* Every 5ms for 100ms */
        lq_isotp_periodic(&channel, mock_can_send, nullptr, t);
    }
    
    /* Should have sent ~4 CFs (one every 20ms over 100ms) */
    size_t cf_count = sent_frames.size() - prev_count;
    EXPECT_GE(cf_count, 4);
    EXPECT_LE(cf_count, 6);
}

TEST_F(ISOTPTest, RX_Timeout) {
    /* FF */
    struct lq_isotp_can_frame ff = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x10, 0x14, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}
    };
    
    ASSERT_EQ(lq_isotp_recv(&channel, &ff, mock_can_send, nullptr, 0), 0);
    
    /* Wait for timeout (default 1000ms) */
    ASSERT_EQ(lq_isotp_periodic(&channel, mock_can_send, nullptr, 1500000), -ETIMEDOUT);
    
    /* Should have aborted reception */
    const uint8_t *rx_data;
    size_t rx_len;
    EXPECT_FALSE(lq_isotp_rx_available(&channel, &rx_data, &rx_len));
}

/* ============================================================================
 * Error Cases
 * ============================================================================ */

TEST_F(ISOTPTest, InvalidSequenceNumber) {
    /* FF */
    struct lq_isotp_can_frame ff = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x10, 0x14, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}
    };
    ASSERT_EQ(lq_isotp_recv(&channel, &ff, mock_can_send, nullptr, 0), 0);
    
    /* CF with wrong SN (expect 1, send 2) */
    struct lq_isotp_can_frame cf = {
        .can_id = 0x7E8,
        .dlc = 8,
        .data = {0x22, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D}  /* SN=2 */
    };
    
    ASSERT_EQ(lq_isotp_recv(&channel, &cf, mock_can_send, nullptr, 1000), -EINVAL);
}

TEST_F(ISOTPTest, MessageTooLarge) {
    uint8_t large_data[5000];
    
    ASSERT_EQ(lq_isotp_send(&channel, large_data, sizeof(large_data),
                           mock_can_send, nullptr, 0), -EMSGSIZE);
}

TEST_F(ISOTPTest, BusyTransmitter) {
    uint8_t data1[20];
    uint8_t data2[10];
    
    ASSERT_EQ(lq_isotp_send(&channel, data1, sizeof(data1), mock_can_send, nullptr, 0), 0);
    
    /* Try to send while already transmitting */
    ASSERT_EQ(lq_isotp_send(&channel, data2, sizeof(data2), mock_can_send, nullptr, 0), -EBUSY);
}

/* ============================================================================
 * UDS over CAN Integration Test
 * ============================================================================ */

TEST(UDSCANTest, BasicIntegration) {
    struct lq_uds_can uds_can;
    
    /* Mock callbacks */
    auto get_seed = [](uint8_t level, uint8_t *seed) -> int {
        (void)level;
        memcpy(seed, "\x12\x34\x56\x78", 4);
        return 0;
    };
    
    auto verify_key = [](uint8_t level, const uint8_t *key) -> bool {
        (void)level;
        return memcmp(key, "\x9A\xBC\xDE\xF0", 4) == 0;
    };
    
    /* UDS config */
    struct lq_uds_config uds_cfg = {};
    uds_cfg.get_seed = get_seed;
    uds_cfg.verify_key = verify_key;
    
    /* ISO-TP config */
    struct lq_isotp_config isotp_cfg = {};
    isotp_cfg.tx_id = 0x7E8;
    isotp_cfg.rx_id = 0x7E0;
    isotp_cfg.use_extended_id = false;
    
    sent_frames.clear();
    
    ASSERT_EQ(lq_uds_can_init(&uds_can, &uds_cfg, &isotp_cfg, mock_can_send, nullptr), 0);
    
    /* Send diagnostic session control request (single frame) */
    struct lq_isotp_can_frame request = {
        .can_id = 0x7E0,
        .dlc = 3,
        .data = {0x02, 0x10, 0x03}  /* SF len=2: Session Extended */
    };
    
    ASSERT_EQ(lq_uds_can_recv_frame(&uds_can, &request, 0), 0);
    
    /* Should have sent positive response */
    ASSERT_GE(sent_frames.size(), 1);
    EXPECT_EQ(sent_frames[0].data[0] & 0xF0, 0x00);  /* Single frame */
    EXPECT_EQ(sent_frames[0].data[1], 0x50);  /* Positive response to 0x10 */
    EXPECT_EQ(sent_frames[0].data[2], 0x03);  /* Extended session */
    
    EXPECT_EQ(lq_uds_can_get_session(&uds_can), UDS_SESSION_EXTENDED_DIAGNOSTIC);
}
