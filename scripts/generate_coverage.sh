#!/bin/bash
# Generate code coverage report locally

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=========================================="
echo "  Code Coverage Report Generator"
echo "=========================================="
echo ""

# Check if lcov is installed
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}Error: lcov is not installed${NC}"
    echo "Install with: sudo apt-get install lcov"
    exit 1
fi

# Check if genhtml is installed
if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}Error: genhtml is not installed${NC}"
    echo "Install with: sudo apt-get install lcov"
    exit 1
fi

# Build directory
BUILD_DIR="${BUILD_DIR:-build}"
COVERAGE_DIR="${BUILD_DIR}/coverage"

echo "Build directory: ${BUILD_DIR}"
echo "Coverage output: ${COVERAGE_DIR}"
echo ""

# Check if build exists
if [ ! -d "${BUILD_DIR}" ]; then
    echo -e "${YELLOW}Build directory not found. Configuring...${NC}"
    cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
fi

# Build
echo -e "${GREEN}Building...${NC}"
cmake --build "${BUILD_DIR}" -j$(nproc)

# Run tests
echo ""
echo -e "${GREEN}Running tests...${NC}"
cd "${BUILD_DIR}"
ctest --output-on-failure
cd ..

# Generate coverage data
echo ""
echo -e "${GREEN}Generating coverage data...${NC}"
lcov --capture \
     --directory "${BUILD_DIR}" \
     --output-file "${BUILD_DIR}/coverage.info" \
     --rc branch_coverage=0 \
     --ignore-errors mismatch,gcov,inconsistent

# Remove unwanted files
echo ""
echo -e "${GREEN}Filtering coverage data...${NC}"
lcov --remove "${BUILD_DIR}/coverage.info" \
     '/usr/*' \
     '*/tests/*' \
     '*/googletest/*' \
     '*/build/_deps/*' \
     '*/samples/*' \
     --ignore-errors unused \
     --output-file "${BUILD_DIR}/coverage_filtered.info"

# Generate HTML report
echo ""
echo -e "${GREEN}Generating HTML report...${NC}"
mkdir -p "${COVERAGE_DIR}"
genhtml "${BUILD_DIR}/coverage_filtered.info" \
        --output-directory "${COVERAGE_DIR}" \
        --title "Layered Queue Driver Coverage" \
        --legend \
        --show-details \
        --demangle-cpp

# Extract coverage percentage
COVERAGE=$(lcov --summary "${BUILD_DIR}/coverage_filtered.info" 2>&1 | grep "lines" | awk '{print $2}' | sed 's/%//')

# Display summary
echo ""
echo "=========================================="
echo "  Coverage Summary"
echo "=========================================="
echo ""
lcov --list "${BUILD_DIR}/coverage_filtered.info"
echo ""
echo "=========================================="

# Color-coded coverage percentage
if (( $(echo "$COVERAGE >= 80" | bc -l) )); then
    COLOR=$GREEN
    STATUS="EXCELLENT"
elif (( $(echo "$COVERAGE >= 70" | bc -l) )); then
    COLOR=$GREEN
    STATUS="GOOD"
elif (( $(echo "$COVERAGE >= 60" | bc -l) )); then
    COLOR=$YELLOW
    STATUS="ACCEPTABLE"
else
    COLOR=$RED
    STATUS="NEEDS IMPROVEMENT"
fi

echo -e "${COLOR}Overall Coverage: ${COVERAGE}% (${STATUS})${NC}"
echo ""
echo "HTML report generated at:"
echo "  file://$(pwd)/${COVERAGE_DIR}/index.html"
echo ""
echo "Open in browser:"
echo "  xdg-open ${COVERAGE_DIR}/index.html"
echo ""
