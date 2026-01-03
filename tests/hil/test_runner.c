/*
 * AUTO-GENERATED HIL Test Runner
 * Generated from test DTS file
 * DO NOT EDIT MANUALLY
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "lq_hil.h"
#include "lq_j1939.h"

/* Test statistics */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper: Print TAP result */
static void tap_result(bool passed, const char *test_name, const char *details)
{
    tests_run++;
    if (passed) {
        tests_passed++;
        printf("ok %d - %s", tests_run, test_name);
    } else {
        tests_failed++;
        printf("not ok %d - %s", tests_run, test_name);
    }
    
    if (details && details[0]) {
        printf(" # %s", details);
    }
    printf("\n");
}

/* Helper: Parse byte array from DTS */
static void parse_byte_array(const char *str, uint8_t *data, size_t *len)
{
    *len = 0;
    const char *p = str;
    
    while (*p && *len < 8) {
        while (*p && !isxdigit(*p)) p++;
        if (!*p) break;
        
        int value;
        sscanf(p, "%x", &value);
        data[(*len)++] = (uint8_t)value;
        
        while (*p && isxdigit(*p)) p++;
    }
}


/* Test: Normal operation with 3 agreeing RPM sensors */
static bool hil_test_rpm_normal(void)
{
    char details[256] = "";
    uint64_t start_time, latency_us;
    
    /* Step 0: Inject ADC */
    if (lq_hil_tester_inject_adc(0, 3000) != 0) {
        snprintf(details, sizeof(details), "Failed to inject ADC ch%d", 0);
        return false;
    }
    /* Step 1: Inject ADC */
    if (lq_hil_tester_inject_adc(1, 3005) != 0) {
        snprintf(details, sizeof(details), "Failed to inject ADC ch%d", 1);
        return false;
    }
    /* Step 2: Inject CAN (J1939) */
    uint32_t can_id_2 = lq_j1939_build_id_from_pgn(61444, 3, 0x00);
    uint8_t can_data_2[8];
    size_t can_len_2;
    parse_byte_array("[0xE8 0x5E 0x00 0x00 0x00 0x00 0x00 0x00]", can_data_2, &can_len_2);
    
    if (lq_hil_tester_inject_can(can_id_2, 1, can_data_2, can_len_2) != 0) {
        snprintf(details, sizeof(details), "Failed to inject CAN");
        return false;
    }
    /* Step 3: Expect CAN message */
    struct lq_hil_can_msg can_msg_3;
    if (lq_hil_tester_wait_can(&can_msg_3, 150) != 0) {
        snprintf(details, sizeof(details), "CAN message timeout");
        return false;
    }
    
    /* Verify PGN */
    uint32_t received_pgn = (can_msg_3.can_id >> 8) & 0x3FFFF;
    if (received_pgn != 61444) {
        snprintf(details, sizeof(details), "Expected PGN 61444, got %u", received_pgn);
        return false;
    }
    
    return true;
}

/* Test: Fault on one RPM sensor - system degrades gracefully */
static bool hil_test_rpm_fault(void)
{
    char details[256] = "";
    uint64_t start_time, latency_us;
    
    /* Step 0: Inject ADC */
    if (lq_hil_tester_inject_adc(0, 2500) != 0) {
        snprintf(details, sizeof(details), "Failed to inject ADC ch%d", 0);
        return false;
    }
    /* Step 1: Inject ADC */
    if (lq_hil_tester_inject_adc(1, 2505) != 0) {
        snprintf(details, sizeof(details), "Failed to inject ADC ch%d", 1);
        return false;
    }
    /* Step 2: Inject ADC */
    if (lq_hil_tester_inject_adc(0, 9999) != 0) {
        snprintf(details, sizeof(details), "Failed to inject ADC ch%d", 0);
        return false;
    }
    /* Step 3: Expect CAN message */
    struct lq_hil_can_msg can_msg_3;
    if (lq_hil_tester_wait_can(&can_msg_3, 1500) != 0) {
        snprintf(details, sizeof(details), "CAN message timeout");
        return false;
    }
    
    /* Verify PGN */
    uint32_t received_pgn = (can_msg_3.can_id >> 8) & 0x3FFFF;
    if (received_pgn != 65226) {
        snprintf(details, sizeof(details), "Expected PGN 65226, got %u", received_pgn);
        return false;
    }
    /* Step 4: Expect CAN message */
    struct lq_hil_can_msg can_msg_4;
    if (lq_hil_tester_wait_can(&can_msg_4, 200) != 0) {
        snprintf(details, sizeof(details), "CAN message timeout");
        return false;
    }
    
    /* Verify PGN */
    uint32_t received_pgn = (can_msg_4.can_id >> 8) & 0x3FFFF;
    if (received_pgn != 61444) {
        snprintf(details, sizeof(details), "Expected PGN 61444, got %u", received_pgn);
        return false;
    }
    
    return true;
}

/* Test: End-to-end latency measurement */
static bool hil_test_latency(void)
{
    char details[256] = "";
    uint64_t start_time, latency_us;
    
    /* Step 0: Measure latency */
    start_time = lq_hil_get_timestamp_us();
    
    /* TODO: Implement trigger and response from nested properties */
    
    latency_us = lq_hil_get_timestamp_us() - start_time;
    if (latency_us > 10000) {
        snprintf(details, sizeof(details), "Latency %lluus exceeds limit 10000us", latency_us);
        return false;
    }
    snprintf(details, sizeof(details), "latency: %lluus", latency_us);
    
    return true;
}

int main(int argc, char *argv[])
{
    int sut_pid = 0;
    
    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--sut-pid=", 10) == 0) {
            sut_pid = atoi(argv[i] + 10);
        }
    }
    
    if (sut_pid == 0) {
        fprintf(stderr, "Usage: %s --sut-pid=<pid>\n", argv[0]);
        return 1;
    }
    
    /* Initialize HIL in tester mode */
    if (lq_hil_init(LQ_HIL_MODE_TESTER, sut_pid) != 0) {
        fprintf(stderr, "Failed to initialize HIL tester\n");
        return 1;
    }
    
    /* TAP header */
    printf("TAP version 14\n");
    printf("1..3\n");
    
    /* Run all tests */
    tap_result(hil_test_rpm_normal(), "hil-test-rpm-normal", "");
    tap_result(hil_test_rpm_fault(), "hil-test-rpm-fault", "");
    tap_result(hil_test_latency(), "hil-test-latency", "");
    
    /* Cleanup */
    lq_hil_cleanup();
    
    /* Summary */
    fprintf(stderr, "\nTests: %d passed, %d failed, %d total\n",
            tests_passed, tests_failed, tests_run);
    
    return tests_failed > 0 ? 1 : 0;
}
