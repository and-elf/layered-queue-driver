#!/bin/bash
# Test compile Arduino examples using arduino-cli

set -e

EXAMPLES_DIR="$(dirname "$0")/examples"
FQBN="arduino:avr:uno"  # Use Uno for basic syntax checking

echo "Testing Arduino example compilation..."
echo "======================================="

for example in "$EXAMPLES_DIR"/*/*.ino; do
    example_name=$(basename "$(dirname "$example")")
    echo -n "Compiling $example_name... "
    
    # Try to compile (will fail but shows us the errors)
    if arduino-cli compile --fqbn "$FQBN" "$example" 2>&1 | grep -E "error|Error"; then
        echo "FAILED"
        exit 1
    else
        echo "OK"
    fi
done

echo ""
echo "All examples compiled successfully!"
