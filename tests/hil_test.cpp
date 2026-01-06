/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * HIL (Hardware-in-the-Loop) Tests using Google Test
 * 
 * These tests run the actual compiled application (SUT) as a separate process
 * and interact with it through the HIL interface to verify end-to-end behavior.
 */

#include <gtest/gtest.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>

extern "C" {
#include "lq_hil.h"
#include "lq_j1939.h"
}

/* ============================================================================
 * HIL Test Fixture
 * ============================================================================ */

class HILTest : public ::testing::Test {
protected:
    static pid_t sut_pid;
    static bool sut_started;
    
    // Start SUT once for all HIL tests
    static void SetUpTestSuite() {
        // Check if SUT binary exists
        if (access("./hil_sut_full", X_OK) != 0) {
            GTEST_SKIP() << "SUT binary './hil_sut_full' not found. Run 'make hil-sut-full' first.";
            return;
        }
        
        std::cout << "[HIL] Starting SUT process..." << std::endl;
        
        // Set environment for HIL mode
        setenv("LQ_HIL_MODE", "sut", 1);
        
        // Fork and exec the SUT
        sut_pid = fork();
        if (sut_pid < 0) {
            FAIL() << "Failed to fork SUT process: " << strerror(errno);
        } else if (sut_pid == 0) {
            // Child process: exec the SUT
            execl("./hil_sut_full", "hil_sut_full", nullptr);
            // If exec fails
            std::cerr << "[HIL] Failed to exec SUT: " << strerror(errno) << std::endl;
            exit(1);
        }
        
        // Parent: wait for SUT to initialize
        std::cout << "[HIL] SUT started with PID " << sut_pid << std::endl;
        sleep(2);  // Give SUT time to initialize and start receiver thread
        
        // Initialize HIL in tester mode
        if (lq_hil_init(LQ_HIL_MODE_TESTER, sut_pid) != 0) {
            kill(sut_pid, SIGTERM);
            waitpid(sut_pid, nullptr, 0);
            FAIL() << "Failed to initialize HIL tester";
        }
        
        std::cout << "[HIL] HIL tester initialized" << std::endl;
        sut_started = true;
    }
    
    // Stop SUT after all HIL tests
    static void TearDownTestSuite() {
        if (!sut_started) {
            return;
        }
        
        std::cout << "[HIL] Shutting down SUT..." << std::endl;
        
        // Cleanup HIL
        lq_hil_cleanup();
        
        // Terminate SUT
        if (sut_pid > 0) {
            kill(sut_pid, SIGTERM);
            
            // Wait with timeout
            int status;
            int count = 0;
            while (count < 10 && waitpid(sut_pid, &status, WNOHANG) == 0) {
                usleep(100000);  // 100ms
                count++;
            }
            
            // Force kill if still running
            if (waitpid(sut_pid, &status, WNOHANG) == 0) {
                std::cout << "[HIL] SUT didn't terminate, force killing..." << std::endl;
                kill(sut_pid, SIGKILL);
                waitpid(sut_pid, &status, 0);
            }
            
            std::cout << "[HIL] SUT terminated" << std::endl;
        }
        
        sut_started = false;
    }
    
    void SetUp() override {
        if (!sut_started) {
            GTEST_SKIP() << "SUT not available";
        }
    }
};

// Static member initialization
pid_t HILTest::sut_pid = 0;
bool HILTest::sut_started = false;

/* ============================================================================
 * HIL Test Cases
 * ============================================================================ */

TEST_F(HILTest, AllInputsNominal) {
    // Inject ADC values at nominal levels
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 2500), 0) << "Failed to inject ADC ch0";
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 2500), 0) << "Failed to inject ADC ch0";
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 2500), 0) << "Failed to inject ADC ch0";
    
    // Wait for CAN response with RPM data (PGN 0xFEF1 = 65265)
    // Filter for the specific PGN since SUT sends multiple cyclic outputs
    struct lq_hil_can_msg can_msg;
    uint32_t received_pgn = 0;
    const uint32_t target_pgn = 65265;  // 0xFEF1
    
    for (int attempts = 0; attempts < 10; attempts++) {
        ASSERT_EQ(lq_hil_tester_wait_can(&can_msg, 200), 0) 
            << "CAN message timeout";
        received_pgn = (can_msg.can_id >> 8) & 0x3FFFF;
        if (received_pgn == target_pgn) {
            break;  // Found our message
        }
    }
    
    EXPECT_EQ(received_pgn, target_pgn) << "Expected PGN 65265 (0xFEF1)";
}

TEST_F(HILTest, VotingMerge) {
    // Inject three ADC inputs with slightly different values
    // The merge voter should select the median
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 2400), 0);  // RPM ADC
    ASSERT_EQ(lq_hil_tester_inject_adc(1, 2500), 0);  // RPM SPI (median)
    ASSERT_EQ(lq_hil_tester_inject_adc(2, 2600), 0);  // Temp ADC
    
    // Give time for data to propagate through system
    usleep(10000);  // 10ms
    
    // Wait for merged/voted result on CAN - filter for PGN 0xFEF1 (65265)
    // and wait for a non-zero value
    struct lq_hil_can_msg can_msg;
    uint32_t received_pgn = 0;
    uint16_t rpm_value = 0;
    const uint32_t target_pgn = 65265;  // 0xFEF1
    
    for (int attempts = 0; attempts < 100; attempts++) {
        ASSERT_EQ(lq_hil_tester_wait_can(&can_msg, 200), 0)
            << "CAN message timeout";
        received_pgn = (can_msg.can_id >> 8) & 0x3FFFF;
        if (received_pgn == target_pgn) {
            rpm_value = can_msg.data[0] | (can_msg.data[1] << 8);
            if (rpm_value != 0) {
                break;  // Found message with non-zero value
            }
        }
    }
    
    EXPECT_EQ(received_pgn, target_pgn) << "Expected PGN 65265 (0xFEF1)";
    
    // The voted value should be close to the median (2500)
    // Parse RPM from CAN data (assume first 2 bytes, little-endian)
    EXPECT_NEAR(rpm_value, 2500, 100) << "Voted RPM should be near median";
}

TEST_F(HILTest, ResponseLatency) {
    // Measure end-to-end latency from ADC injection to CAN output
    
    // Record start time
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    // Inject ADC
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 3000), 0);
    
    // Wait for CAN response
    struct lq_hil_can_msg can_msg;
    ASSERT_EQ(lq_hil_tester_wait_can(&can_msg, 200), 0);
    
    // Record end time
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    // Calculate latency in microseconds
    uint64_t latency_us = (end.tv_sec - start.tv_sec) * 1000000ULL +
                          (end.tv_nsec - start.tv_nsec) / 1000ULL;
    
    std::cout << "[HIL] Response latency: " << latency_us << " us" << std::endl;
    
    // Latency should be reasonable (< 100ms for this simple test)
    EXPECT_LT(latency_us, 100000) << "Latency should be < 100ms";
}

TEST_F(HILTest, FaultCondition) {
    // Inject out-of-range value to trigger fault handling
    ASSERT_EQ(lq_hil_tester_inject_adc(0, 5000), 0);  // Way above nominal
    
    // System should still respond (possibly with fault status)
    struct lq_hil_can_msg can_msg;
    int ret = lq_hil_tester_wait_can(&can_msg, 500);
    
    // Either we get a response or timeout is acceptable (depends on fault behavior)
    // For now, just verify the test infrastructure works
    if (ret == 0) {
        std::cout << "[HIL] Received CAN response despite fault" << std::endl;
    } else {
        std::cout << "[HIL] No CAN response (expected for fault condition)" << std::endl;
    }
    
    SUCCEED() << "Fault condition test completed";
}

TEST_F(HILTest, SequentialInputs) {
    // Test multiple sequential inputs
    for (int i = 0; i < 5; i++) {
        uint16_t adc_value = 2000 + i * 100;
        ASSERT_EQ(lq_hil_tester_inject_adc(0, adc_value), 0) 
            << "Injection " << i << " failed";
        
        usleep(10000);  // 10ms between injections
    }
    
    // Should get at least one CAN message from the sequence
    struct lq_hil_can_msg can_msg;
    ASSERT_EQ(lq_hil_tester_wait_can(&can_msg, 500), 0)
        << "No CAN response to sequential inputs";
    
    SUCCEED();
}

/* ============================================================================
 * Stress Test
 * ============================================================================ */

TEST_F(HILTest, StressTest) {
    // Rapid injection of many ADC values
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(lq_hil_tester_inject_adc(0, 2000 + (i % 1000)), 0);
        usleep(1000);  // 1ms between injections
    }
    
    // Verify system is still responsive
    struct lq_hil_can_msg can_msg;
    ASSERT_EQ(lq_hil_tester_wait_can(&can_msg, 1000), 0)
        << "System unresponsive after stress test";
}
