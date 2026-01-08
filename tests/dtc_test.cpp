/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * DTC Manager Unit Tests
 */

#include <gtest/gtest.h>

extern "C" {
#include "lq_dtc.h"
}

class DTCTest : public ::testing::Test {
protected:
    struct lq_dtc_manager mgr;
    
    void SetUp() override {
        lq_dtc_init(&mgr, 1000);
    }
};

TEST_F(DTCTest, Initialization) {
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 0);
    EXPECT_EQ(lq_dtc_get_stored_count(&mgr), 0);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_OFF);
}

TEST_F(DTCTest, SetActiveDTC) {
    uint64_t now = 1000000;  // 1 second
    
    // Set a DTC active
    int ret = lq_dtc_set_active(&mgr, 1234, LQ_FMI_DATA_VALID_ABOVE_NORMAL, 
                                LQ_LAMP_AMBER, now);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 1);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_AMBER);
}

TEST_F(DTCTest, DTCOccurrenceCount) {
    uint64_t now = 1000000;
    
    // Set same DTC multiple times
    lq_dtc_set_active(&mgr, 5678, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now);
    lq_dtc_set_active(&mgr, 5678, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now + 100000);
    lq_dtc_set_active(&mgr, 5678, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now + 200000);
    
    // Should still be 1 active DTC with occurrence count = 3
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 1);
    
    // Verify in DTC list
    bool found = false;
    for (int i = 0; i < LQ_MAX_DTCS; i++) {
        if (mgr.dtcs[i].state == LQ_DTC_CONFIRMED && mgr.dtcs[i].spn == 5678) {
            EXPECT_EQ(mgr.dtcs[i].occurrence_count, 3);
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DTCTest, ClearDTC) {
    uint64_t now = 1000000;
    
    // Set and clear a DTC
    lq_dtc_set_active(&mgr, 1234, LQ_FMI_DATA_VALID_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 1);
    
    int ret = lq_dtc_clear(&mgr, 1234, LQ_FMI_DATA_VALID_ABOVE_NORMAL, now);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 0);
    EXPECT_EQ(lq_dtc_get_stored_count(&mgr), 1);  // Moved to stored
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_OFF);  // Lamp off
}

TEST_F(DTCTest, ClearNonexistentDTC) {
    int ret = lq_dtc_clear(&mgr, 9999, LQ_FMI_NOT_AVAILABLE, 0);
    EXPECT_EQ(ret, -2);  // -ENOENT
}

TEST_F(DTCTest, ClearAllDTCs) {
    uint64_t now = 1000000;
    
    // Set multiple DTCs
    lq_dtc_set_active(&mgr, 1111, LQ_FMI_DATA_VALID_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
    lq_dtc_set_active(&mgr, 2222, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now);
    lq_dtc_set_active(&mgr, 3333, LQ_FMI_DATA_ERRATIC, LQ_LAMP_AMBER_FLASH, now);
    
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 3);
    
    // Clear all
    lq_dtc_clear_all(&mgr);
    
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), 0);
    EXPECT_EQ(lq_dtc_get_stored_count(&mgr), 0);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_OFF);
}

TEST_F(DTCTest, MILStatusPriority) {
    uint64_t now = 1000000;
    
    // Add DTCs with different lamp severities
    lq_dtc_set_active(&mgr, 1111, LQ_FMI_DATA_VALID_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_AMBER);
    
    lq_dtc_set_active(&mgr, 2222, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_RED);  // Red is highest
    
    lq_dtc_set_active(&mgr, 3333, LQ_FMI_DATA_ERRATIC, LQ_LAMP_AMBER_FLASH, now);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_RED);  // Still red
    
    // Clear red fault
    lq_dtc_clear(&mgr, 2222, LQ_FMI_VOLTAGE_BELOW_NORMAL, now);
    EXPECT_EQ(lq_dtc_get_mil_status(&mgr), LQ_LAMP_AMBER_FLASH);  // Now flash
}

TEST_F(DTCTest, DM1MessageEmpty) {
    uint8_t data[64];
    uint64_t now = 1000000;
    
    // Build DM1 with no DTCs
    int len = lq_dtc_build_dm1(&mgr, data, sizeof(data), now);
    
    EXPECT_EQ(len, 8);  // Minimum DM1 size
    EXPECT_EQ(data[0], 0x00);  // All lamps off
    EXPECT_EQ(data[1], 0x00);
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x00);
    EXPECT_EQ(data[4], 0xFF);  // No DTC indicator
    EXPECT_EQ(data[5], 0xFF);
    EXPECT_EQ(data[6], 0xFF);
    EXPECT_EQ(data[7], 0xFF);
}

TEST_F(DTCTest, DM1MessageWithDTC) {
    uint8_t data[64];
    uint64_t now = 1000000;
    
    // Set a DTC
    lq_dtc_set_active(&mgr, 1234, LQ_FMI_DATA_VALID_ABOVE_NORMAL, LQ_LAMP_AMBER, now);
    
    int len = lq_dtc_build_dm1(&mgr, data, sizeof(data), now);
    
    EXPECT_EQ(len, 8);  // Header + 1 DTC = 8 bytes
    
    // Check lamp status (amber warning)
    EXPECT_EQ(data[0], 0xC0);  // Protect lamp on
    
    // Check DTC encoding (SPN=1234, FMI=0)
    // SPN 1234 = 0x04D2
    // [D2, 04, (00 << 3) | (00 & 0x1F), occurrence_count]
    EXPECT_EQ(data[4], 0xD2);  // SPN low byte
    EXPECT_EQ(data[5], 0x04);  // SPN mid byte
    EXPECT_EQ(data[6], (0 << 3) | (LQ_FMI_DATA_VALID_ABOVE_NORMAL & 0x1F));  // SPN high + FMI
    EXPECT_EQ(data[7], 1);     // Occurrence count
}

TEST_F(DTCTest, DM1RateLimiting) {
    uint8_t data[64];
    uint64_t now = 1000000;
    
    // First send should work
    int len1 = lq_dtc_build_dm1(&mgr, data, sizeof(data), now);
    EXPECT_GT(len1, 0);
    
    // Immediate retry should be rate-limited
    int len2 = lq_dtc_build_dm1(&mgr, data, sizeof(data), now + 100);
    EXPECT_EQ(len2, 0);  // Too soon
    
    // After period expires, should work again
    int len3 = lq_dtc_build_dm1(&mgr, data, sizeof(data), now + 1000000);  // +1 second
    EXPECT_GT(len3, 0);
}

TEST_F(DTCTest, DM2MessageWithStoredDTC) {
    uint8_t data[64];
    uint64_t now = 1000000;
    
    // Set and clear a DTC (moves to stored)
    lq_dtc_set_active(&mgr, 5678, LQ_FMI_VOLTAGE_BELOW_NORMAL, LQ_LAMP_RED, now);
    lq_dtc_clear(&mgr, 5678, LQ_FMI_VOLTAGE_BELOW_NORMAL, now);
    
    int len = lq_dtc_build_dm2(&mgr, data, sizeof(data));
    
    EXPECT_EQ(len, 8);
    
    // DM2 should have no lamps
    EXPECT_EQ(data[0], 0x00);
    EXPECT_EQ(data[1], 0x00);
    EXPECT_EQ(data[2], 0x00);
    EXPECT_EQ(data[3], 0x00);
    
    // Should have the stored DTC
    // SPN 5678 = 0x162E
    EXPECT_EQ(data[4], 0x2E);
    EXPECT_EQ(data[5], 0x16);
    EXPECT_EQ(data[6], (0 << 3) | (LQ_FMI_VOLTAGE_BELOW_NORMAL & 0x1F));
    EXPECT_EQ(data[7], 1);
}

TEST_F(DTCTest, MaxDTCs) {
    uint64_t now = 1000000;
    
    // Fill up DTC table
    for (int i = 0; i < LQ_MAX_DTCS; i++) {
        int ret = lq_dtc_set_active(&mgr, (uint32_t)(1000 + i), LQ_FMI_DATA_ERRATIC, 
                                    LQ_LAMP_AMBER, now);
        EXPECT_EQ(ret, 0);
    }
    
    EXPECT_EQ(lq_dtc_get_active_count(&mgr), LQ_MAX_DTCS);
    
    // Try to add one more - should fail
    int ret = lq_dtc_set_active(&mgr, 9999, LQ_FMI_DATA_ERRATIC, LQ_LAMP_AMBER, now);
    EXPECT_EQ(ret, -12);  // -ENOMEM
}
