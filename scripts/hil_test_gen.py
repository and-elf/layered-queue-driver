#!/usr/bin/env python3
"""
HIL Test DTS to C Test Runner Generator

Parses HIL test DTS files and generates a C test runner that:
- Executes test sequences
- Injects inputs via HIL sockets
- Validates outputs
- Measures timing/latency
- Outputs TAP (Test Anything Protocol) format

Usage:
    python3 scripts/hil_test_gen.py <test.dts> <output_dir>

Example:
    python3 scripts/hil_test_gen.py tests/rpm_test.dts tests/
    # Generates: tests/test_runner.c
"""

import sys
import re
from pathlib import Path

class TestStep:
    def __init__(self, step_num):
        self.step_num = step_num
        self.action = ""
        self.properties = {}

class HILTest:
    def __init__(self, name):
        self.name = name
        self.description = ""
        self.timeout_ms = 5000
        self.steps = []

def parse_test_dts(dts_content):
    """Parse test DTS and extract all hil-test nodes"""
    tests = []
    
    # Find all hil-test-* nodes
    test_pattern = r'(hil-test-[\w-]+)\s*\{'
    matches = re.finditer(test_pattern, dts_content)
    
    for match in matches:
        test_name = match.group(1)
        test = HILTest(test_name)
        
        # Extract test block
        start = match.end()
        depth = 1
        end = start
        
        for i in range(start, len(dts_content)):
            if dts_content[i] == '{':
                depth += 1
            elif dts_content[i] == '}':
                depth -= 1
                if depth == 0:
                    end = i
                    break
        
        test_block = dts_content[start:end]
        
        # Parse description
        desc_match = re.search(r'description\s*=\s*"([^"]+)"', test_block)
        if desc_match:
            test.description = desc_match.group(1)
        
        # Parse timeout
        timeout_match = re.search(r'timeout-ms\s*=\s*<(\d+)>', test_block)
        if timeout_match:
            test.timeout_ms = int(timeout_match.group(1))
        
        # Parse sequence steps - find balanced braces
        sequence_start = test_block.find('sequence')
        if sequence_start >= 0:
            brace_start = test_block.find('{', sequence_start)
            if brace_start >= 0:
                # Find matching closing brace
                depth = 1
                pos = brace_start + 1
                while pos < len(test_block) and depth > 0:
                    if test_block[pos] == '{':
                        depth += 1
                    elif test_block[pos] == '}':
                        depth -= 1
                    pos += 1
                
                sequence_block = test_block[brace_start+1:pos-1]
            else:
                sequence_block = ""
        else:
            sequence_block = ""
        
        if sequence_block:
            # Find all step@N nodes
            step_pattern = r'step@(\d+)\s*\{'
            step_matches = list(re.finditer(step_pattern, sequence_block))
            
            for i, step_match in enumerate(step_matches):
                step_num = int(step_match.group(1))
                step_start = step_match.end()
                
                # Find step content (until next step or end)
                if i + 1 < len(step_matches):
                    step_end = step_matches[i+1].start()
                else:
                    step_end = len(sequence_block)
                
                # Extract balanced braces for this step
                depth = 1
                pos = step_start
                while pos < step_end and depth > 0:
                    if sequence_block[pos] == '{':
                        depth += 1
                    elif sequence_block[pos] == '}':
                        depth -= 1
                    pos += 1
                
                step_content = sequence_block[step_start:pos-1]
                
                step = TestStep(step_num)
                
                # Parse action
                action_match = re.search(r'action\s*=\s*"([^"]+)"', step_content)
                if action_match:
                    step.action = action_match.group(1)
                
                # Parse all properties
                prop_pattern = r'([\w-]+)\s*=\s*(?:<([^>]+)>|"([^"]+)"|(\[[\s\w]+\]))'
                for prop_match in re.finditer(prop_pattern, step_content):
                    prop_name = prop_match.group(1)
                    if prop_match.group(2):  # <value>
                        step.properties[prop_name] = prop_match.group(2).strip()
                    elif prop_match.group(3):  # "string"
                        step.properties[prop_name] = prop_match.group(3)
                    elif prop_match.group(4):  # [array]
                        step.properties[prop_name] = prop_match.group(4)
                
                test.steps.append(step)
        
        tests.append(test)
    
    return tests

def generate_test_runner(tests, output_file):
    """Generate C++ GTest test runner from parsed tests"""
    
    code = """/*
 * AUTO-GENERATED HIL Test Runner (GTest)
 * Generated from test DTS file
 * DO NOT EDIT MANUALLY
 */

#include <gtest/gtest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>

extern "C" {
#include "lq_hil.h"
#include "lq_j1939.h"
}

/* Global SUT PID for communication */
static int g_sut_pid = 0;

/* GTest Fixture for HIL Tests */
class HILTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize HIL in tester mode
        ASSERT_NE(g_sut_pid, 0) << "SUT PID not set";
        ASSERT_EQ(lq_hil_init(LQ_HIL_MODE_TESTER, NULL, g_sut_pid), 0)
            << "Failed to initialize HIL tester";
    }
    
    void TearDown() override {
        lq_hil_cleanup();
    }
    
    /* Helper: Parse byte array from DTS */
    static void parse_byte_array(const char *str, uint8_t *data, size_t *len) {
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
};

"""
    
    # Generate each test as a GTest TEST_F
    for test in tests:
        test_name = test.name.replace('-', '_').replace('hil_test_', '')
        
        code += f"""
/* Test: {test.description} */
TEST_F(HILTest, {test_name})
{{
    uint64_t start_time, latency_us;
    
"""
        
        for step in test.steps:
            action = step.action
            
            if action == "inject-adc":
                channel = step.properties.get('channel', '0')
                value = step.properties.get('value', '0')
                delay_ms = step.properties.get('delay-ms', '0')
                
                code += f"""    /* Step {step.step_num}: Inject ADC */
    ASSERT_EQ(lq_hil_tester_inject_adc({channel}, {value}), 0)
        << "Failed to inject ADC on channel " << {channel};
"""
                if int(delay_ms) > 0:
                    code += f"    usleep({delay_ms} * 1000);\n"
            
            elif action == "inject-can" or action == "inject-can-pgn":
                if action == "inject-can-pgn":
                    pgn = step.properties.get('pgn', '0')
                    priority = step.properties.get('priority', '6')
                    source = step.properties.get('source-addr', '0x28')
                    code += f"""    /* Step {step.step_num}: Inject CAN (J1939) */
    uint32_t can_id_{step.step_num} = lq_j1939_build_id_from_pgn({pgn}, {priority}, {source});
"""
                else:
                    can_id = step.properties.get('can-id', '0')
                    code += f"    uint32_t can_id_{step.step_num} = {can_id};\n"
                
                extended = step.properties.get('extended', '1')
                data_str = step.properties.get('data', '[0x00]')
                
                code += f"""    uint8_t can_data_{step.step_num}[8];
    size_t can_len_{step.step_num};
    parse_byte_array("{data_str}", can_data_{step.step_num}, &can_len_{step.step_num});
    
    ASSERT_EQ(lq_hil_tester_inject_can(can_id_{step.step_num}, {extended}, 
                                        can_data_{step.step_num}, can_len_{step.step_num}), 0)
        << "Failed to inject CAN message";
"""
            
            elif action == "wait-gpio-high" or action == "wait-gpio-low":
                pin = step.properties.get('pin', '0')
                timeout_ms = step.properties.get('timeout-ms', '1000')
                expected_state = '1' if action == "wait-gpio-high" else '0'
                
                code += f"""    /* Step {step.step_num}: Wait for GPIO */
    ASSERT_EQ(lq_hil_tester_wait_gpio(NULL, {pin}, {expected_state}, {timeout_ms}), 0)
        << "GPIO pin " << {pin} << " timeout";
"""
            
            elif action == "expect-can":
                timeout_ms = step.properties.get('timeout-ms', '1000')
                pgn = step.properties.get('pgn', None)
                
                code += f"""    /* Step {step.step_num}: Expect CAN message */
    struct lq_hil_can_msg can_msg_{step.step_num};
    ASSERT_EQ(lq_hil_tester_wait_can(&can_msg_{step.step_num}, {timeout_ms}), 0)
        << "CAN message timeout";
"""
                
                if pgn:
                    code += f"""    
    /* Verify PGN */
    uint32_t received_pgn = (can_msg_{step.step_num}.can_id >> 8) & 0x3FFFF;
    EXPECT_EQ(received_pgn, {pgn})
        << "Expected PGN " << {pgn} << ", got " << received_pgn;
"""
            
            elif action == "measure-latency":
                max_latency = step.properties.get('max-latency-us', '50000')
                
                code += f"""    /* Step {step.step_num}: Measure latency */
    start_time = lq_hil_get_timestamp_us();
    
    /* TODO: Implement trigger and response from nested properties */
    
    latency_us = lq_hil_get_timestamp_us() - start_time;
    EXPECT_LE(latency_us, {max_latency})
        << "Latency " << latency_us << "us exceeds limit {max_latency}us";
"""
            
            elif action == "delay":
                duration_ms = step.properties.get('duration-ms', '100')
                code += f"    usleep({duration_ms} * 1000);\n"
        
        code += """}
"""
    
    # Generate main function with GTest initialization
    code += f"""
int main(int argc, char **argv)
{{
    /* Parse SUT PID before GTest consumes arguments */
    for (int i = 1; i < argc; i++) {{
        if (strncmp(argv[i], "--sut-pid=", 10) == 0) {{
            g_sut_pid = atoi(argv[i] + 10);
        }}
    }}
    
    if (g_sut_pid == 0) {{
        fprintf(stderr, "Error: --sut-pid=<pid> argument required\\n");
        return 1;
    }}
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}}
"""
    
    # Write to file
    with open(output_file, 'w') as f:
        f.write(code)
    
    print(f"Generated test runner: {output_file}")
    print(f"  Tests: {len(tests)}")
    for test in tests:
        print(f"    - {test.name}: {len(test.steps)} steps")

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    
    dts_file = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    
    if not dts_file.exists():
        print(f"Error: {dts_file} not found")
        sys.exit(1)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Read and parse DTS
    with open(dts_file) as f:
        dts_content = f.read()
    
    tests = parse_test_dts(dts_content)
    
    if not tests:
        print(f"Warning: No HIL tests found in {dts_file}")
        sys.exit(0)
    
    # Generate test runner (C++ with GTest)
    output_file = output_dir / "test_runner.cpp"
    generate_test_runner(tests, output_file)

if __name__ == '__main__':
    main()
