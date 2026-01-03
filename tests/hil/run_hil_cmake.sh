#!/bin/bash
# HIL Test Runner for CMake
# Args: $1=SUT_PATH, $2=TEST_RUNNER_PATH

set -e

SUT_PATH="$1"
TEST_RUNNER_PATH="$2"

if [ ! -x "$SUT_PATH" ]; then
    echo "Error: SUT not found at $SUT_PATH"
    exit 1
fi

if [ ! -x "$TEST_RUNNER_PATH" ]; then
    echo "Error: Test runner not found at $TEST_RUNNER_PATH"
    exit 1
fi

# Start SUT in background
echo "Starting SUT..."
LQ_HIL_MODE=sut "$SUT_PATH" > /tmp/hil_sut.log 2>&1 &
SUT_PID=$!

echo "SUT PID: $SUT_PID"
sleep 1

# Check if SUT is still running
if ! kill -0 $SUT_PID 2>/dev/null; then
    echo "Error: SUT failed to start"
    cat /tmp/hil_sut.log
    exit 1
fi

# Run tests
echo
"$TEST_RUNNER_PATH" --sut-pid=$SUT_PID
TEST_RESULT=$?

# Cleanup
echo
kill $SUT_PID 2>/dev/null || true
wait $SUT_PID 2>/dev/null || true
rm -f /tmp/lq_hil_*_$SUT_PID 2>/dev/null || true

echo
if [ $TEST_RESULT -eq 0 ]; then
    echo "✓ All HIL tests passed!"
else
    echo "✗ HIL tests failed"
    echo
    echo "=== SUT Log ==="
    cat /tmp/hil_sut.log
fi

exit $TEST_RESULT
