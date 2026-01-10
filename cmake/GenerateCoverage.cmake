# Core Library Coverage Report Generator (Cross-Platform)
# Runs unit tests only and generates coverage for core library
#
# Usage: cmake -P cmake/GenerateCoverage.cmake
# Or via target: cmake --build build --target coverage

cmake_minimum_required(VERSION 3.14)

# Check for required tools
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)

if(NOT LCOV_EXECUTABLE)
    message(FATAL_ERROR "lcov not found. Install with: sudo apt-get install lcov (Linux) or brew install lcov (macOS)")
endif()

if(NOT GENHTML_EXECUTABLE)
    message(FATAL_ERROR "genhtml not found. Install with: sudo apt-get install lcov (Linux) or brew install lcov (macOS)")
endif()

# Configuration
if(NOT DEFINED BUILD_DIR)
    set(BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/../build")
endif()

if(NOT DEFINED SOURCE_DIR)
    set(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

get_filename_component(BUILD_DIR "${BUILD_DIR}" ABSOLUTE)
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)

set(COVERAGE_DIR "${BUILD_DIR}/coverage")

message(STATUS "==========================================")
message(STATUS "  Code Coverage Report Generator")
message(STATUS "==========================================")
message(STATUS "")
message(STATUS "Build directory: ${BUILD_DIR}")
message(STATUS "Coverage output: ${COVERAGE_DIR}")
message(STATUS "")

# Check if build exists
if(NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
    message(STATUS "Build directory not found. Configuring...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B "${BUILD_DIR}" -S "${SOURCE_DIR}"
            -DCMAKE_BUILD_TYPE=Debug
            -DENABLE_COVERAGE=ON
        RESULT_VARIABLE result
    )
    if(result)
        message(FATAL_ERROR "Configuration failed")
    endif()
endif()

# Build
message(STATUS "Building...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" --parallel
    RESULT_VARIABLE result
)
if(result)
    message(FATAL_ERROR "Build failed")
endif()

# Run tests
message(STATUS "")
message(STATUS "Running tests...")
execute_process(
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    WORKING_DIRECTORY "${BUILD_DIR}"
    RESULT_VARIABLE result
)
if(result)
    message(WARNING "Some tests failed")
endif()

# Generate coverage data
message(STATUS "")
message(STATUS "Generating coverage data...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --capture
        --directory "${BUILD_DIR}"
        --output-file "${BUILD_DIR}/coverage.info"
        --rc branch_coverage=0
        --ignore-errors mismatch,gcov,inconsistent
    OUTPUT_QUIET
)

# Remove unwanted files
message(STATUS "")
message(STATUS "Filtering coverage data...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --remove "${BUILD_DIR}/coverage.info"
        '/usr/*'
        '*/tests/*'
        '*/googletest/*'
        '*/build/_deps/*'
        '*/samples/*'
        --ignore-errors unused
        --output-file "${BUILD_DIR}/coverage_filtered.info"
    OUTPUT_QUIET
)

# Generate HTML report
message(STATUS "")
message(STATUS "Generating HTML report...")
file(MAKE_DIRECTORY "${COVERAGE_DIR}")
execute_process(
    COMMAND ${GENHTML_EXECUTABLE} "${BUILD_DIR}/coverage_filtered.info"
        --output-directory "${COVERAGE_DIR}"
        --title "Layered Queue Driver Coverage"
        --legend
        --show-details
        --demangle-cpp
    OUTPUT_QUIET
)

# Extract coverage percentage
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --summary "${BUILD_DIR}/coverage_filtered.info"
    OUTPUT_VARIABLE SUMMARY_OUTPUT
    ERROR_VARIABLE SUMMARY_OUTPUT
)

# Display summary
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  Coverage Summary")
message(STATUS "==========================================")
message(STATUS "")
message(STATUS "${SUMMARY_OUTPUT}")
message(STATUS "==========================================")

# Parse coverage percentage
string(REGEX MATCH "lines[.]*: ([0-9]+\\.[0-9]+)%" COVERAGE_MATCH "${SUMMARY_OUTPUT}")
if(CMAKE_MATCH_1)
    set(COVERAGE "${CMAKE_MATCH_1}")
    
    # Status
    if(COVERAGE GREATER_EQUAL 80)
        set(STATUS "EXCELLENT")
    elseif(COVERAGE GREATER_EQUAL 70)
        set(STATUS "GOOD")
    elseif(COVERAGE GREATER_EQUAL 60)
        set(STATUS "ACCEPTABLE")
    else()
        set(STATUS "NEEDS IMPROVEMENT")
    endif()
    
    message(STATUS "")
    message(STATUS "Overall Coverage: ${COVERAGE}% (${STATUS})")
endif()

message(STATUS "")
message(STATUS "HTML report generated at:")
message(STATUS "  file://${COVERAGE_DIR}/index.html")
message(STATUS "")
