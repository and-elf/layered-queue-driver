#!/bin/bash
# CI HIL Test Runner
# Runs HIL tests against multiple real hardware boards connected via USB
#
# Usage: ./ci_hil_runner.sh [board1] [board2] ...
# Example: ./ci_hil_runner.sh stm32f4 esp32 nrf52

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Hardware board configurations
declare -A BOARD_TRANSPORT
declare -A BOARD_DEVICE
declare -A BOARD_BAUDRATE
declare -A BOARD_FLASH_CMD

# STM32F4 Discovery
BOARD_TRANSPORT[stm32f4]="uart"
BOARD_DEVICE[stm32f4]="/dev/serial/by-id/usb-STMicroelectronics_STM32_*"
BOARD_BAUDRATE[stm32f4]="115200"
BOARD_FLASH_CMD[stm32f4]="openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c 'program $BUILD_DIR/firmware.elf verify reset exit'"

# ESP32 DevKit
BOARD_TRANSPORT[esp32]="uart"
BOARD_DEVICE[esp32]="/dev/serial/by-id/usb-Silicon_Labs_CP2102_*"
BOARD_BAUDRATE[esp32]="115200"
BOARD_FLASH_CMD[esp32]="esptool.py --port \${BOARD_DEVICE[esp32]} write_flash 0x0 $BUILD_DIR/firmware.bin"

# nRF52840 DK
BOARD_TRANSPORT[nrf52]="uart"
BOARD_DEVICE[nrf52]="/dev/serial/by-id/usb-SEGGER_J-Link_*"
BOARD_BAUDRATE[nrf52]="115200"
BOARD_FLASH_CMD[nrf52]="nrfjprog --program $BUILD_DIR/firmware.hex --chiperase --reset"

echo "==================================================================="
echo "CI Hardware-in-Loop Test Runner"
echo "==================================================================="
echo

# Default to all boards if none specified
BOARDS=("$@")
if [ ${#BOARDS[@]} -eq 0 ]; then
    BOARDS=(stm32f4 esp32 nrf52)
fi

echo "Testing boards: ${BOARDS[*]}"
echo

TOTAL_PASSED=0
TOTAL_FAILED=0
RESULTS=""

for BOARD in "${BOARDS[@]}"; do
    echo "-------------------------------------------------------------------"
    echo "Testing: $BOARD"
    echo "-------------------------------------------------------------------"
    
    if [ -z "${BOARD_DEVICE[$BOARD]}" ]; then
        echo "❌ Unknown board: $BOARD"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        RESULTS="${RESULTS}❌ $BOARD - Unknown board\n"
        continue
    fi
    
    # Find actual device (handle wildcards)
    DEVICE=$(ls ${BOARD_DEVICE[$BOARD]} 2>/dev/null | head -1)
    
    if [ -z "$DEVICE" ]; then
        echo "⚠️  Board not connected: $BOARD (${BOARD_DEVICE[$BOARD]})"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        RESULTS="${RESULTS}⚠️  $BOARD - Not connected\n"
        continue
    fi
    
    echo "Device: $DEVICE"
    echo "Transport: ${BOARD_TRANSPORT[$BOARD]}"
    echo "Baudrate: ${BOARD_BAUDRATE[$BOARD]}"
    echo
    
    # Build for platform
    echo "Building firmware for $BOARD..."
    cd "$BUILD_DIR"
    cmake .. -DLQ_PLATFORM=$BOARD -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
    make -j$(nproc) >/dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "❌ Build failed for $BOARD"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        RESULTS="${RESULTS}❌ $BOARD - Build failed\n"
        continue
    fi
    
    echo "✓ Build successful"
    
    # Flash firmware
    echo "Flashing firmware to $BOARD..."
    eval "${BOARD_FLASH_CMD[$BOARD]}" >/dev/null 2>&1
    
    if [ $? -ne 0 ]; then
        echo "❌ Flash failed for $BOARD"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        RESULTS="${RESULTS}❌ $BOARD - Flash failed\n"
        continue
    fi
    
    echo "✓ Flash successful"
    sleep 2  # Wait for board to boot
    
    # Run HIL tests
    echo "Running HIL tests..."
    TEST_OUTPUT=$("$BUILD_DIR/hil_test_runner" \
        --transport=${BOARD_TRANSPORT[$BOARD]} \
        --device="$DEVICE" \
        --baudrate=${BOARD_BAUDRATE[$BOARD]} 2>&1)
    
    TEST_RESULT=$?
    
    echo "$TEST_OUTPUT"
    echo
    
    # Parse TAP output
    PASSED=$(echo "$TEST_OUTPUT" | grep -c "^ok ")
    FAILED=$(echo "$TEST_OUTPUT" | grep -c "^not ok ")
    
    if [ $TEST_RESULT -eq 0 ]; then
        echo "✅ $BOARD: All tests passed ($PASSED/$((PASSED + FAILED)))"
        TOTAL_PASSED=$((TOTAL_PASSED + 1))
        RESULTS="${RESULTS}✅ $BOARD - Passed ($PASSED tests)\n"
    else
        echo "❌ $BOARD: Tests failed ($PASSED passed, $FAILED failed)"
        TOTAL_FAILED=$((TOTAL_FAILED + 1))
        RESULTS="${RESULTS}❌ $BOARD - Failed ($FAILED/$((PASSED + FAILED)) failed)\n"
    fi
    
    echo
done

# Summary
echo "==================================================================="
echo "CI HIL Test Summary"
echo "==================================================================="
echo -e "$RESULTS"
echo "Total: $TOTAL_PASSED passed, $TOTAL_FAILED failed"
echo

if [ $TOTAL_FAILED -eq 0 ]; then
    echo "🎉 All boards passed!"
    exit 0
else
    echo "💥 Some boards failed"
    exit 1
fi
