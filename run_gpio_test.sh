#!/bin/bash
# Quick test runner for GPIO threshold test

set -e

cd "$(dirname "$0")"
ROOT="$(pwd)"

echo "=== GPIO Threshold Test ==="
echo "Building SUT..."

# Generate code if needed
python3 scripts/dts_gen.py samples/basic/gpio_threshold_test.dts build/gpio_test

# Compile SUT
gcc -o build/gpio_test/sut \
    -I include -I build/gpio_test \
    -DLQ_PLATFORM_NATIVE -DENABLE_HIL_TESTS \
    build/gpio_test/lq_generated.c \
    build/gpio_test/main.c \
    src/drivers/*.c \
    src/platform/lq_platform_hil.c \
    src/platform/lq_platform_stubs.c \
    -lpthread -lm

echo "Compiling test runner..."

# Compile test runner  
gcc -o build/gpio_test/test_runner \
    -I include -I build/gpio_test \
    -DLQ_PLATFORM_NATIVE -DENABLE_HIL_TESTS \
    build/gpio_test/test_runner.c \
    src/drivers/lq_hil.c \
    -lpthread -lm

echo ""
echo "=== Running HIL Tests ==="
echo "Starting SUT in background..."

# Run SUT in background
LQ_HIL_MODE=sut build/gpio_test/sut &
SUT_PID=$!

# Give SUT time to start
sleep 1

echo "Running test harness..."

# Run tests
LQ_HIL_MODE=test build/gpio_test/test_runner
TEST_RESULT=$?

# Cleanup
kill $SUT_PID 2>/dev/null || true

if [ $TEST_RESULT -eq 0 ]; then
    echo ""
    echo "✅ All tests PASSED!"
else
    echo ""
    echo "❌ Tests FAILED"
fi

exit $TEST_RESULT
