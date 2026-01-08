/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * Configuration Framework Tests
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "lq_config.h"
#include "lq_engine.h"
#include "lq_uds.h"
}

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&engine, 0, sizeof(engine));
        memset(remaps, 0, sizeof(remaps));
        memset(scales, 0, sizeof(scales));
        
        lq_config_init(&registry, &engine, remaps, MAX_REMAPS, scales, MAX_SCALES);
    }
    
    void TearDown() override {
    }
    
    static constexpr uint8_t MAX_REMAPS = 4;
    static constexpr uint8_t MAX_SCALES = 4;
    
    lq_config_registry registry;
    lq_engine engine;
    lq_remap_ctx remaps[MAX_REMAPS];
    lq_scale_ctx scales[MAX_SCALES];
};

/* ============================================================================
 * Basic Configuration Tests
 * ============================================================================ */

TEST_F(ConfigTest, InitRegistry) {
    EXPECT_EQ(registry.engine, &engine);
    EXPECT_EQ(registry.remaps, remaps);
    EXPECT_EQ(registry.scales, scales);
    EXPECT_EQ(registry.max_remaps, MAX_REMAPS);
    EXPECT_EQ(registry.max_scales, MAX_SCALES);
    EXPECT_EQ(registry.num_remaps, 0);
    EXPECT_EQ(registry.num_scales, 0);
    EXPECT_FALSE(registry.config_locked);
    EXPECT_FALSE(registry.calibration_mode);
    EXPECT_EQ(registry.config_version, 0);
}

TEST_F(ConfigTest, ReadSignal) {
    /* Set up test signal */
    engine.signals[5].value = 1234;
    engine.signals[5].status = LQ_EVENT_OK;
    
    int32_t value;
    uint8_t status;
    
    int ret = lq_config_read_signal(&registry, 5, &value, &status);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(value, 1234);
    EXPECT_EQ(status, (uint8_t)LQ_EVENT_OK);
}

TEST_F(ConfigTest, ReadSignalInvalidId) {
    int32_t value;
    uint8_t status;
    
    int ret = lq_config_read_signal(&registry, LQ_MAX_SIGNALS, &value, &status);
    EXPECT_EQ(ret, -ENOENT);
}

/* ============================================================================
 * Remap Configuration Tests
 * ============================================================================ */

TEST_F(ConfigTest, AddRemap) {
    lq_remap_ctx remap = {
        .input_signal = 1,
        .output_signal = 2,
        .invert = true,
        .deadzone = 100,
        .enabled = true,
    };
    
    uint8_t index;
    int ret = lq_config_add_remap(&registry, &remap, &index);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(index, 0);
    EXPECT_EQ(registry.num_remaps, 1);
    EXPECT_EQ(registry.config_version, 1);
    
    /* Verify stored config */
    EXPECT_EQ(remaps[0].input_signal, 1);
    EXPECT_EQ(remaps[0].output_signal, 2);
    EXPECT_TRUE(remaps[0].invert);
    EXPECT_EQ(remaps[0].deadzone, 100);
    EXPECT_TRUE(remaps[0].enabled);
}

TEST_F(ConfigTest, AddRemapInvalidSignal) {
    lq_remap_ctx remap = {
        .input_signal = LQ_MAX_SIGNALS,  /* Invalid */
        .output_signal = 2,
        .invert = false,
        .deadzone = 0,
        .enabled = true,
    };
    
    uint8_t index;
    int ret = lq_config_add_remap(&registry, &remap, &index);
    EXPECT_EQ(ret, -LQ_NRC_REQUEST_OUT_OF_RANGE);
    EXPECT_EQ(registry.num_remaps, 0);
}

TEST_F(ConfigTest, AddRemapFull) {
    lq_remap_ctx remap = {
        .input_signal = 1,
        .output_signal = 2,
        .invert = false,
        .deadzone = 0,
        .enabled = true,
    };
    
    /* Fill up remaps */
    for (int i = 0; i < MAX_REMAPS; i++) {
        uint8_t index;
        EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    }
    
    /* Try to add one more */
    uint8_t index;
    int ret = lq_config_add_remap(&registry, &remap, &index);
    EXPECT_EQ(ret, -ENOMEM);
}

TEST_F(ConfigTest, ReadWriteRemap) {
    /* Add initial remap */
    lq_remap_ctx remap1 = {
        .input_signal = 1,
        .output_signal = 2,
        .invert = false,
        .deadzone = 0,
        .enabled = true,
    };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_remap(&registry, &remap1, &index), 0);
    EXPECT_EQ(index, 0);
    
    /* Read it back */
    lq_remap_ctx read_remap;
    EXPECT_EQ(lq_config_read_remap(&registry, 0, &read_remap), 0);
    EXPECT_EQ(read_remap.input_signal, 1);
    EXPECT_EQ(read_remap.output_signal, 2);
    
    /* Modify it */
    lq_remap_ctx remap2 = {
        .input_signal = 3,
        .output_signal = 4,
        .invert = true,
        .deadzone = 50,
        .enabled = false,
    };
    
    EXPECT_EQ(lq_config_write_remap(&registry, 0, &remap2), 0);
    EXPECT_EQ(registry.config_version, 2);  /* Incremented by add and write */
    
    /* Read modified version */
    EXPECT_EQ(lq_config_read_remap(&registry, 0, &read_remap), 0);
    EXPECT_EQ(read_remap.input_signal, 3);
    EXPECT_EQ(read_remap.output_signal, 4);
    EXPECT_TRUE(read_remap.invert);
    EXPECT_EQ(read_remap.deadzone, 50);
    EXPECT_FALSE(read_remap.enabled);
}

TEST_F(ConfigTest, RemoveRemap) {
    /* Add 3 remaps */
    for (int i = 0; i < 3; i++) {
        lq_remap_ctx remap = {
            .input_signal = static_cast<uint8_t>(i),
            .output_signal = static_cast<uint8_t>(i + 10),
            .invert = false,
            .deadzone = 0,
            .enabled = true,
        };
        uint8_t index;
        EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    }
    
    EXPECT_EQ(registry.num_remaps, 3);
    
    /* Remove middle one */
    EXPECT_EQ(lq_config_remove_remap(&registry, 1), 0);
    EXPECT_EQ(registry.num_remaps, 2);
    
    /* Verify remaining configs shifted */
    lq_remap_ctx remap;
    EXPECT_EQ(lq_config_read_remap(&registry, 0, &remap), 0);
    EXPECT_EQ(remap.input_signal, 0);
    
    EXPECT_EQ(lq_config_read_remap(&registry, 1, &remap), 0);
    EXPECT_EQ(remap.input_signal, 2);  /* Was index 2, now index 1 */
}

/* ============================================================================
 * Scale Configuration Tests
 * ============================================================================ */

TEST_F(ConfigTest, AddScale) {
    lq_scale_ctx scale = {
        .input_signal = 5,
        .output_signal = 6,
        .scale_factor = 1000,
        .offset = -500,
        .clamp_min = -1000,
        .clamp_max = 1000,
        .has_clamp_min = true,
        .has_clamp_max = true,
        .enabled = true,
    };
    
    uint8_t index;
    int ret = lq_config_add_scale(&registry, &scale, &index);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(index, 0);
    EXPECT_EQ(registry.num_scales, 1);
    
    /* Verify stored config */
    EXPECT_EQ(scales[0].input_signal, 5);
    EXPECT_EQ(scales[0].output_signal, 6);
    EXPECT_EQ(scales[0].scale_factor, 1000);
    EXPECT_EQ(scales[0].offset, -500);
    EXPECT_TRUE(scales[0].has_clamp_min);
    EXPECT_TRUE(scales[0].has_clamp_max);
}

TEST_F(ConfigTest, AddScaleInvalidClamp) {
    lq_scale_ctx scale = {
        .input_signal = 1,
        .output_signal = 2,
        .scale_factor = 100,
        .offset = 0,
        .clamp_min = 1000,   /* Min > Max */
        .clamp_max = -1000,
        .has_clamp_min = true,
        .has_clamp_max = true,
        .enabled = true,
    };
    
    uint8_t index;
    int ret = lq_config_add_scale(&registry, &scale, &index);
    EXPECT_EQ(ret, -LQ_NRC_REQUEST_OUT_OF_RANGE);
}

TEST_F(ConfigTest, ReadWriteScale) {
    lq_scale_ctx scale1 = {
        .input_signal = 1,
        .output_signal = 2,
        .scale_factor = 100,
        .offset = 0,
        .clamp_min = 0,
        .clamp_max = 0,
        .has_clamp_min = false,
        .has_clamp_max = false,
        .enabled = true,
    };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_scale(&registry, &scale1, &index), 0);
    
    /* Read and modify */
    lq_scale_ctx read_scale;
    EXPECT_EQ(lq_config_read_scale(&registry, 0, &read_scale), 0);
    EXPECT_EQ(read_scale.scale_factor, 100);
    
    lq_scale_ctx scale2 = scale1;
    scale2.scale_factor = 200;
    scale2.offset = 50;
    
    EXPECT_EQ(lq_config_write_scale(&registry, 0, &scale2), 0);
    
    EXPECT_EQ(lq_config_read_scale(&registry, 0, &read_scale), 0);
    EXPECT_EQ(read_scale.scale_factor, 200);
    EXPECT_EQ(read_scale.offset, 50);
}

/* ============================================================================
 * Calibration Mode Tests
 * ============================================================================ */

TEST_F(ConfigTest, CalibrationMode) {
    EXPECT_FALSE(registry.calibration_mode);
    EXPECT_FALSE(registry.config_locked);
    
    /* Enter calibration */
    EXPECT_EQ(lq_config_enter_calibration_mode(&registry), 0);
    EXPECT_TRUE(registry.calibration_mode);
    
    /* Add config while in calibration mode */
    lq_remap_ctx remap = {
        .input_signal = 1,
        .output_signal = 2,
        .invert = false,
        .deadzone = 0,
        .enabled = true,
    };
    uint8_t index;
    EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    
    /* Exit calibration */
    EXPECT_EQ(lq_config_exit_calibration_mode(&registry), 0);
    EXPECT_FALSE(registry.calibration_mode);
    EXPECT_TRUE(registry.config_locked);
    
    /* Try to modify after lock */
    EXPECT_EQ(lq_config_write_remap(&registry, 0, &remap), -LQ_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(ConfigTest, ResetToDefaults) {
    /* Add some configs */
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    lq_scale_ctx scale = { .input_signal = 3, .output_signal = 4, .scale_factor = 100, .offset = 0, 
                           .clamp_min = 0, .clamp_max = 0, .has_clamp_min = false, .has_clamp_max = false, .enabled = true };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    EXPECT_EQ(lq_config_add_scale(&registry, &scale, &index), 0);
    EXPECT_EQ(registry.num_remaps, 1);
    EXPECT_EQ(registry.num_scales, 1);
    
    /* Reset */
    EXPECT_EQ(lq_config_reset_to_defaults(&registry), 0);
    EXPECT_EQ(registry.num_remaps, 0);
    EXPECT_EQ(registry.num_scales, 0);
}

/* ============================================================================
 * UDS DID Handler Tests
 * ============================================================================ */

TEST_F(ConfigTest, UdsReadSignalValue) {
    /* Set up test signal */
    engine.signals[10].value = 0x12345678;
    
    uint8_t data[10];
    size_t actual_len;
    
    /* Request format: [signal_id] */
    data[0] = 10;
    
    int ret = lq_config_uds_read_did(&registry, LQ_DID_SIGNAL_VALUE, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual_len, 4);
    
    /* Response format: [value_hh][value_hl][value_lh][value_ll] */
    EXPECT_EQ(data[0], 0x12);
    EXPECT_EQ(data[1], 0x34);
    EXPECT_EQ(data[2], 0x56);
    EXPECT_EQ(data[3], 0x78);
}

TEST_F(ConfigTest, UdsReadSignalStatus) {
    engine.signals[15].status = LQ_EVENT_OK;
    
    uint8_t data[10];
    size_t actual_len;
    
    data[0] = 15;
    
    int ret = lq_config_uds_read_did(&registry, LQ_DID_SIGNAL_STATUS, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual_len, 1);
    EXPECT_EQ(data[0], (uint8_t)LQ_EVENT_OK);
}

TEST_F(ConfigTest, UdsReadRemapConfig) {
    /* Add remap */
    lq_remap_ctx remap = {
        .input_signal = 7,
        .output_signal = 8,
        .invert = true,
        .deadzone = 0x1234,
        .enabled = true,
    };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    
    /* Read via UDS DID */
    uint8_t data[10];
    size_t actual_len;
    
    data[0] = 0;  /* Index */
    
    int ret = lq_config_uds_read_did(&registry, LQ_DID_REMAP_CONFIG, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual_len, 5);
    
    /* Verify serialized format */
    EXPECT_EQ(data[0], 7);      /* input_signal */
    EXPECT_EQ(data[1], 8);      /* output_signal */
    EXPECT_EQ(data[2], 0x03);   /* flags: invert=1, enabled=1 */
    EXPECT_EQ(data[3], 0x12);   /* deadzone high */
    EXPECT_EQ(data[4], 0x34);   /* deadzone low */
}

TEST_F(ConfigTest, UdsWriteRemapConfig) {
    /* Add initial remap */
    lq_remap_ctx remap = {
        .input_signal = 1,
        .output_signal = 2,
        .invert = false,
        .deadzone = 0,
        .enabled = true,
    };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_remap(&registry, &remap, &index), 0);
    
    /* Write via UDS DID */
    uint8_t data[10];
    data[0] = 0;      /* Index */
    data[1] = 5;      /* input_signal */
    data[2] = 6;      /* output_signal */
    data[3] = 0x01;   /* invert=1, enabled=0 */
    data[4] = 0x00;   /* deadzone high */
    data[5] = 0x64;   /* deadzone low (100) */
    
    int ret = lq_config_uds_write_did(&registry, LQ_DID_REMAP_CONFIG, data, 6);
    EXPECT_EQ(ret, 0);
    
    /* Verify update */
    lq_remap_ctx read_remap;
    EXPECT_EQ(lq_config_read_remap(&registry, 0, &read_remap), 0);
    EXPECT_EQ(read_remap.input_signal, 5);
    EXPECT_EQ(read_remap.output_signal, 6);
    EXPECT_TRUE(read_remap.invert);
    EXPECT_FALSE(read_remap.enabled);
    EXPECT_EQ(read_remap.deadzone, 100);
}

TEST_F(ConfigTest, UdsReadScaleConfig) {
    lq_scale_ctx scale = {
        .input_signal = 10,
        .output_signal = 11,
        .scale_factor = 0x1234,
        .offset = 0x5678,
        .clamp_min = 0x11223344,
        .clamp_max = 0x55667788,
        .has_clamp_min = true,
        .has_clamp_max = true,
        .enabled = true,
    };
    
    uint8_t index;
    EXPECT_EQ(lq_config_add_scale(&registry, &scale, &index), 0);
    
    uint8_t data[20];
    size_t actual_len;
    
    data[0] = 0;
    
    int ret = lq_config_uds_read_did(&registry, LQ_DID_SCALE_CONFIG, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual_len, 15);
    
    /* Verify serialization */
    EXPECT_EQ(data[0], 10);     /* input_signal */
    EXPECT_EQ(data[1], 11);     /* output_signal */
    EXPECT_EQ(data[2], 0x07);   /* enabled | has_clamp_min | has_clamp_max */
    EXPECT_EQ(data[3], 0x12);   /* scale_factor high */
    EXPECT_EQ(data[4], 0x34);   /* scale_factor low */
    EXPECT_EQ(data[5], 0x56);   /* offset high */
    EXPECT_EQ(data[6], 0x78);   /* offset low */
}

TEST_F(ConfigTest, UdsReadCalibrationMode) {
    uint8_t data[10];
    size_t actual_len;
    
    int ret = lq_config_uds_read_did(&registry, LQ_DID_CALIBRATION_MODE, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(actual_len, 8);
    
    EXPECT_EQ(data[0], 0);  /* calibration_mode = false */
    EXPECT_EQ(data[1], 0);  /* config_locked = false */
    EXPECT_EQ(data[2], 0);  /* num_remaps */
    EXPECT_EQ(data[3], 0);  /* num_scales */
    
    /* Enter calibration, add configs */
    lq_config_enter_calibration_mode(&registry);
    
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    uint8_t index;
    lq_config_add_remap(&registry, &remap, &index);
    
    ret = lq_config_uds_read_did(&registry, LQ_DID_CALIBRATION_MODE, data, sizeof(data), &actual_len);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(data[0], 1);  /* calibration_mode = true */
    EXPECT_EQ(data[2], 1);  /* num_remaps = 1 */
}

TEST_F(ConfigTest, UdsRoutineEnterCalibration) {
    uint8_t out_data[10];
    size_t actual_out;
    
    int ret = lq_config_uds_routine_control(&registry, LQ_RID_ENTER_CALIBRATION,
                                            LQ_UDS_ROUTINE_START, nullptr, 0,
                                            out_data, sizeof(out_data), &actual_out);
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(registry.calibration_mode);
}

TEST_F(ConfigTest, UdsRoutineExitCalibration) {
    lq_config_enter_calibration_mode(&registry);
    
    uint8_t out_data[10];
    size_t actual_out;
    
    int ret = lq_config_uds_routine_control(&registry, LQ_RID_EXIT_CALIBRATION,
                                            LQ_UDS_ROUTINE_START, nullptr, 0,
                                            out_data, sizeof(out_data), &actual_out);
    EXPECT_EQ(ret, 0);
    EXPECT_FALSE(registry.calibration_mode);
    EXPECT_TRUE(registry.config_locked);
}

TEST_F(ConfigTest, UdsRoutineResetDefaults) {
    /* Add config */
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    uint8_t index;
    lq_config_add_remap(&registry, &remap, &index);
    
    uint8_t out_data[10];
    size_t actual_out;
    
    int ret = lq_config_uds_routine_control(&registry, LQ_RID_RESET_DEFAULTS,
                                            LQ_UDS_ROUTINE_START, nullptr, 0,
                                            out_data, sizeof(out_data), &actual_out);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(registry.num_remaps, 0);
}

// ============================================================================
// NULL Pointer and Edge Case Tests
// ============================================================================

TEST_F(ConfigTest, InitNullRegistry) {
    int ret = lq_config_init(nullptr, &engine, remaps, MAX_REMAPS, scales, MAX_SCALES);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, InitNullEngine) {
    lq_config_registry reg;
    int ret = lq_config_init(&reg, nullptr, remaps, MAX_REMAPS, scales, MAX_SCALES);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadSignalNullRegistry) {
    int32_t value;
    uint8_t status;
    int ret = lq_config_read_signal(nullptr, 0, &value, &status);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadSignalNullValue) {
    uint8_t status;
    int ret = lq_config_read_signal(&registry, 0, nullptr, &status);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadSignalNullStatus) {
    int32_t value;
    int ret = lq_config_read_signal(&registry, 0, &value, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadRemapNullRegistry) {
    lq_remap_ctx remap;
    int ret = lq_config_read_remap(nullptr, 0, &remap);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadRemapNullOutput) {
    int ret = lq_config_read_remap(&registry, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadRemapInvalidIndex) {
    lq_remap_ctx remap;
    int ret = lq_config_read_remap(&registry, MAX_REMAPS, &remap);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, WriteRemapNullRegistry) {
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    int ret = lq_config_write_remap(nullptr, 0, &remap);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, WriteRemapNullRemap) {
    int ret = lq_config_write_remap(&registry, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, WriteRemapInvalidIndex) {
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    int ret = lq_config_write_remap(&registry, MAX_REMAPS, &remap);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, WriteRemapConfigLocked) {
    lq_remap_ctx remap1 = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    uint8_t index;
    lq_config_add_remap(&registry, &remap1, &index);
    
    registry.config_locked = true;
    
    lq_remap_ctx remap2 = { .input_signal = 3, .output_signal = 4, .invert = true, .deadzone = 100, .enabled = false };
    int ret = lq_config_write_remap(&registry, 0, &remap2);
    EXPECT_EQ(ret, -LQ_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(ConfigTest, ReadScaleNullRegistry) {
    lq_scale_ctx scale;
    int ret = lq_config_read_scale(nullptr, 0, &scale);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadScaleNullOutput) {
    int ret = lq_config_read_scale(&registry, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ReadScaleInvalidIndex) {
    lq_scale_ctx scale;
    int ret = lq_config_read_scale(&registry, MAX_SCALES, &scale);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, WriteScaleNullRegistry) {
    lq_scale_ctx scale = { .input_signal = 1, .output_signal = 2, .scale_factor = 100, .offset = 0, .enabled = true };
    int ret = lq_config_write_scale(nullptr, 0, &scale);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, WriteScaleNullScale) {
    int ret = lq_config_write_scale(&registry, 0, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, WriteScaleInvalidIndex) {
    lq_scale_ctx scale = { .input_signal = 1, .output_signal = 2, .scale_factor = 100, .offset = 0, .enabled = true };
    int ret = lq_config_write_scale(&registry, MAX_SCALES, &scale);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, WriteScaleConfigLocked) {
    lq_scale_ctx scale1 = { .input_signal = 1, .output_signal = 2, .scale_factor = 100, .offset = 0, .enabled = true };
    uint8_t index;
    lq_config_add_scale(&registry, &scale1, &index);
    
    registry.config_locked = true;
    
    lq_scale_ctx scale2 = { .input_signal = 3, .output_signal = 4, .scale_factor = 200, .offset = 50, .enabled = false };
    int ret = lq_config_write_scale(&registry, 0, &scale2);
    EXPECT_EQ(ret, -LQ_NRC_SECURITY_ACCESS_DENIED);
}

TEST_F(ConfigTest, AddRemapNullRegistry) {
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    uint8_t index;
    int ret = lq_config_add_remap(nullptr, &remap, &index);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, AddRemapNullRemap) {
    uint8_t index;
    int ret = lq_config_add_remap(&registry, nullptr, &index);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, AddRemapNullIndex) {
    lq_remap_ctx remap = { .input_signal = 1, .output_signal = 2, .invert = false, .deadzone = 0, .enabled = true };
    int ret = lq_config_add_remap(&registry, &remap, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, AddScaleNullRegistry) {
    lq_scale_ctx scale = { .input_signal = 1, .output_signal = 2, .scale_factor = 100, .offset = 0, .enabled = true };
    uint8_t index;
    int ret = lq_config_add_scale(nullptr, &scale, &index);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, AddScaleNullScale) {
    uint8_t index;
    int ret = lq_config_add_scale(&registry, nullptr, &index);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, AddScaleNullIndex) {
    lq_scale_ctx scale = { .input_signal = 1, .output_signal = 2, .scale_factor = 100, .offset = 0, .enabled = true };
    int ret = lq_config_add_scale(&registry, &scale, nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, RemoveRemapNullRegistry) {
    int ret = lq_config_remove_remap(nullptr, 0);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, RemoveRemapInvalidIndex) {
    int ret = lq_config_remove_remap(&registry, MAX_REMAPS);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, RemoveScaleNullRegistry) {
    int ret = lq_config_remove_scale(nullptr, 0);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, RemoveScaleInvalidIndex) {
    int ret = lq_config_remove_scale(&registry, MAX_SCALES);
    EXPECT_EQ(ret, -ENOENT);
}

TEST_F(ConfigTest, EnterCalibrationModeNullRegistry) {
    int ret = lq_config_enter_calibration_mode(nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ExitCalibrationModeNullRegistry) {
    int ret = lq_config_exit_calibration_mode(nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, ResetDefaultsNullRegistry) {
    int ret = lq_config_reset_to_defaults(nullptr);
    EXPECT_EQ(ret, -EINVAL);
}

TEST_F(ConfigTest, CalibrationModeAlreadyActive) {
    lq_config_enter_calibration_mode(&registry);
    EXPECT_TRUE(registry.calibration_mode);
    
    int ret = lq_config_enter_calibration_mode(&registry);
    EXPECT_EQ(ret, 0);  // Should succeed but no change
    EXPECT_TRUE(registry.calibration_mode);
}

TEST_F(ConfigTest, ExitCalibrationModeNotActive) {
    EXPECT_FALSE(registry.calibration_mode);
    
    int ret = lq_config_exit_calibration_mode(&registry);
    EXPECT_EQ(ret, 0);  // Succeeds even if not in calibration mode
    EXPECT_FALSE(registry.calibration_mode);
    EXPECT_TRUE(registry.config_locked);
}
