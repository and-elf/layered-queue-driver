#!/bin/bash
# HIL Test Runner Script

set -e

cd "$(dirname "$0")/../.."

echo "=== HIL Test Rig ==="
echo

# Start SUT
echo "Starting SUT..."
LQ_HIL_MODE=sut tests/hil/simple_sut &
SUT_PID=$!

echo "SUT PID: $SUT_PID"
sleep 1

# Run tests
echo
echo "Running HIL tests..."
tests/hil/test_runner --sut-pid=$SUT_PID
TEST_RESULT=$?

# Cleanup
echo
echo "Cleaning up..."
kill $SUT_PID 2>/dev/null || true
wait $SUT_PID 2>/dev/null || true

# Clean up sockets
rm -f /tmp/lq_hil_*_$SUT_PID 2>/dev/null || true

echo
if [ $TEST_RESULT -eq 0 ]; then
    echo "✓ All tests passed!"
else
    echo "✗ Tests failed"
fi

exit $TEST_RESULT
