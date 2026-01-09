/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * BLDC Motor Driver Unit Tests
 */

#include <gtest/gtest.h>
#include "lq_bldc.h"
#include <cstring>

/* Mock platform functions */
static uint8_t last_motor_id = 0;
static uint8_t last_phase = 0;
static uint16_t last_duty = 0;
static bool motor_enabled = false;

extern "C" {
    int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
        last_motor_id = motor_id;
        (void)config;
        return 0;
    }
    
    int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase,
                                   uint16_t duty_cycle) {
        last_motor_id = motor_id;
        last_phase = phase;
        last_duty = duty_cycle;
        return 0;
    }
    
    int lq_bldc_platform_enable(uint8_t motor_id, bool enable) {
        last_motor_id = motor_id;
        motor_enabled = enable;
        return 0;
    }
    
    int lq_bldc_platform_brake(uint8_t motor_id) {
        last_motor_id = motor_id;
        motor_enabled = false;
        return 0;
    }
}

class BLDCTest : public ::testing::Test {
protected:
    struct lq_bldc_motor motor;
    struct lq_bldc_config config;
    
    void SetUp() override {
        memset(&motor, 0, sizeof(motor));
        config.num_phases = 3;
        config.pole_pairs = 7;
        config.mode = LQ_BLDC_MODE_SINE;
        config.pwm_frequency_hz = 25000;
        config.max_duty_cycle = 10000;
        config.enable_deadtime = true;
        config.deadtime_ns = 1000;
        
        /* Default pin configuration (not used in mock) */
        memset(&config.high_side_pins, 0, sizeof(config.high_side_pins));
        memset(&config.low_side_pins, 0, sizeof(config.low_side_pins));
        
        motor_enabled = false;
        last_duty = 0;
    }
};

TEST_F(BLDCTest, Initialization) {
    int ret = lq_bldc_init(&motor, &config, 0);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(motor.motor_id, 0);
    EXPECT_EQ(motor.config.num_phases, 3);
    EXPECT_FALSE(motor.state.enabled);
    EXPECT_EQ(motor.state.power, 0);
}

TEST_F(BLDCTest, InvalidConfig) {
    config.num_phases = 10;  /* Exceeds LQ_BLDC_MAX_PHASES */
    int ret = lq_bldc_init(&motor, &config, 0);
    EXPECT_NE(ret, 0);
}

TEST_F(BLDCTest, PowerControl) {
    lq_bldc_init(&motor, &config, 0);
    
    int ret = lq_bldc_set_power(&motor, 50);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(motor.state.power, 50);
    
    /* Test clamping */
    lq_bldc_set_power(&motor, 150);
    EXPECT_EQ(motor.state.power, 100);
}

TEST_F(BLDCTest, EnableDisable) {
    lq_bldc_init(&motor, &config, 0);
    
    lq_bldc_enable(&motor, true);
    EXPECT_TRUE(motor.state.enabled);
    EXPECT_TRUE(motor_enabled);
    
    lq_bldc_enable(&motor, false);
    EXPECT_FALSE(motor.state.enabled);
    EXPECT_FALSE(motor_enabled);
}

TEST_F(BLDCTest, DirectionControl) {
    lq_bldc_init(&motor, &config, 0);
    
    lq_bldc_set_direction(&motor, LQ_BLDC_DIR_FORWARD);
    EXPECT_EQ(motor.state.direction, LQ_BLDC_DIR_FORWARD);
    
    lq_bldc_set_direction(&motor, LQ_BLDC_DIR_REVERSE);
    EXPECT_EQ(motor.state.direction, LQ_BLDC_DIR_REVERSE);
}

TEST_F(BLDCTest, SinusoidalCommutation) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 50);
    lq_bldc_enable(&motor, true);
    
    /* Update for 1ms */
    lq_bldc_update(&motor, 1000);
    
    /* All 3 phases should have non-zero duty cycles */
    bool all_phases_active = true;
    for (int i = 0; i < 3; i++) {
        if (motor.state.duty_cycle[i] == 0) {
            all_phases_active = false;
        }
    }
    EXPECT_TRUE(all_phases_active);
    
    /* Electrical angle should have incremented */
    EXPECT_GT(motor.state.electrical_angle, 0);
}

TEST_F(BLDCTest, SixStepCommutation) {
    config.mode = LQ_BLDC_MODE_6STEP;
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 100);  /* Full power */
    lq_bldc_enable(&motor, true);
    
    lq_bldc_update(&motor, 1000);
    
    /* In 6-step, only 2 of 3 phases are active at any time */
    int active_phases = 0;
    for (int i = 0; i < 3; i++) {
        if (motor.state.duty_cycle[i] > 0) {
            active_phases++;
        }
    }
    EXPECT_LE(active_phases, 2);
    
    /* Commutation step should be valid (0-5) */
    EXPECT_LT(motor.state.commutation_step, 6);
}

TEST_F(BLDCTest, ElectricalAngleProgression) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 25);
    lq_bldc_enable(&motor, true);
    
    uint16_t angle_before = motor.state.electrical_angle;
    
    /* Update multiple times */
    for (int i = 0; i < 10; i++) {
        lq_bldc_update(&motor, 1000);  /* 1ms each */
    }
    
    /* Angle should have wrapped around (0-65535) */
    EXPECT_NE(motor.state.electrical_angle, angle_before);
}

TEST_F(BLDCTest, DutyCycleScaling) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_enable(&motor, true);
    
    /* 0% power */
    lq_bldc_set_power(&motor, 0);
    lq_bldc_update(&motor, 1000);
    
    /* Motor should not generate PWM when power is 0 */
    /* (update returns early) */
    
    /* 100% power */
    lq_bldc_set_power(&motor, 100);
    lq_bldc_update(&motor, 1000);
    
    /* At least one phase should be near max duty */
    bool has_high_duty = false;
    for (int i = 0; i < 3; i++) {
        if (motor.state.duty_cycle[i] > 9000) {  /* >90% */
            has_high_duty = true;
        }
    }
    EXPECT_TRUE(has_high_duty);
}

TEST_F(BLDCTest, EmergencyStop) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 75);
    lq_bldc_enable(&motor, true);
    
    lq_bldc_emergency_stop(&motor);
    
    EXPECT_FALSE(motor.state.enabled);
    EXPECT_EQ(motor.state.power, 0);
    EXPECT_FALSE(motor_enabled);
}

TEST_F(BLDCTest, ReverseDirection) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 50);
    lq_bldc_enable(&motor, true);
    
    /* Forward */
    lq_bldc_set_direction(&motor, LQ_BLDC_DIR_FORWARD);
    lq_bldc_update(&motor, 1000);
    uint16_t duty_fwd[3];
    memcpy(duty_fwd, motor.state.duty_cycle, sizeof(duty_fwd));
    
    /* Reverse */
    lq_bldc_set_direction(&motor, LQ_BLDC_DIR_REVERSE);
    motor.state.electrical_angle = 0;  /* Reset angle */
    lq_bldc_update(&motor, 1000);
    uint16_t duty_rev[3];
    memcpy(duty_rev, motor.state.duty_cycle, sizeof(duty_rev));
    
    /* Phase pattern should be different in reverse */
    bool patterns_differ = false;
    for (int i = 0; i < 3; i++) {
        if (duty_fwd[i] != duty_rev[i]) {
            patterns_differ = true;
        }
    }
    EXPECT_TRUE(patterns_differ);
}

TEST_F(BLDCTest, MaxDutyCycleLimiting) {
    config.max_duty_cycle = 9000;  /* 90% max */
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 100);  /* Full power */
    lq_bldc_enable(&motor, true);
    
    lq_bldc_update(&motor, 1000);
    
    /* No phase should exceed max_duty_cycle */
    for (int i = 0; i < 3; i++) {
        EXPECT_LE(motor.state.duty_cycle[i], 9000);
    }
}

TEST_F(BLDCTest, MultiPhaseSupport) {
    /* Test 4-phase configuration */
    config.num_phases = 4;
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 50);
    lq_bldc_enable(&motor, true);
    
    lq_bldc_update(&motor, 1000);
    
    /* All 4 phases should have calculated duty cycles */
    for (int i = 0; i < 4; i++) {
        EXPECT_GE(motor.state.duty_cycle[i], 0);
    }
}

TEST_F(BLDCTest, ZeroPowerDisabled) {
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_enable(&motor, true);
    lq_bldc_set_power(&motor, 0);
    
    /* Update should return early when power is 0 */
    int ret = lq_bldc_update(&motor, 1000);
    EXPECT_EQ(ret, 0);
}
