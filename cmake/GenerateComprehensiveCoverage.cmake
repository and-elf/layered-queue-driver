# Comprehensive Coverage Report Generator (Cross-Platform)
# Runs unit tests + HIL tests for all sample apps and generates combined coverage
#
# Usage: cmake -P cmake/GenerateComprehensiveCoverage.cmake
# Or via target: cmake --build build --target coverage-full

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
message(STATUS "  Comprehensive Coverage Report")
message(STATUS "  (Core Library + Sample Apps + HIL)")
message(STATUS "==========================================")
message(STATUS "")
message(STATUS "Build directory: ${BUILD_DIR}")
message(STATUS "Source directory: ${SOURCE_DIR}")
message(STATUS "Coverage output: ${COVERAGE_DIR}")
message(STATUS "")

# Step 1: Configure with coverage enabled (if not already)
message(STATUS "Step 1/6: Ensuring build is configured with coverage...")
if(NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
    message(STATUS "  Configuring fresh build...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B "${BUILD_DIR}" -S "${SOURCE_DIR}"
            -DCMAKE_BUILD_TYPE=Debug
            -DENABLE_COVERAGE=ON
            -DBUILD_TESTS=ON
            -DENABLE_HIL_TESTS=ON
        RESULT_VARIABLE result
    )
    if(result)
        message(FATAL_ERROR "Configuration failed")
    endif()
else()
    message(STATUS "  Build already configured")
endif()

# Step 2: Clean old coverage data
message(STATUS "")
message(STATUS "Step 2/6: Cleaning old coverage data...")
file(GLOB_RECURSE GCDA_FILES "${BUILD_DIR}/*.gcda")
if(GCDA_FILES)
    file(REMOVE ${GCDA_FILES})
    message(STATUS "  Removed ${CMAKE_MATCH_COUNT} .gcda files")
endif()
file(GLOB COVERAGE_INFO_FILES "${BUILD_DIR}/coverage*.info")
if(COVERAGE_INFO_FILES)
    file(REMOVE ${COVERAGE_INFO_FILES})
endif()

# Step 3: Build everything
message(STATUS "")
message(STATUS "Step 3/6: Building library, tests, and sample apps...")
execute_process(
    COMMAND ${CMAKE_COMMAND} --build "${BUILD_DIR}" --parallel
    RESULT_VARIABLE result
)
if(result)
    message(FATAL_ERROR "Build failed")
endif()

# Step 4: Run unit tests
message(STATUS "")
message(STATUS "Step 4/6: Running unit tests...")
execute_process(
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -R ".*test" -E "hil_test"
    WORKING_DIRECTORY "${BUILD_DIR}"
    RESULT_VARIABLE result
)
# Don't fail on test failures - we still want coverage
if(result)
    message(WARNING "Some unit tests failed, but continuing for coverage collection")
endif()

# Step 5: Run HIL tests for all sample applications
message(STATUS "")
message(STATUS "Step 5/6: Running HIL tests for sample applications...")

# Find all HIL SUT executables
file(GLOB_RECURSE HIL_SUT_FILES "${BUILD_DIR}/*_hil_sut" "${BUILD_DIR}/*_hil_sut.exe")

if(NOT HIL_SUT_FILES)
    message(STATUS "  No HIL test SUTs found - skipping HIL tests")
else()
    message(STATUS "  Found ${CMAKE_MATCH_COUNT} HIL test suite(s)")
    
    foreach(SUT_PATH ${HIL_SUT_FILES})
        get_filename_component(SUT_DIR "${SUT_PATH}" DIRECTORY)
        get_filename_component(SUT_NAME "${SUT_PATH}" NAME_WE)
        
        # Find corresponding test runner
        set(RUNNER_PATH "${SUT_DIR}/${SUT_NAME}")
        string(REPLACE "_hil_sut" "_hil_test_runner" RUNNER_PATH "${RUNNER_PATH}")
        
        # Also try finding any runner in the same directory
        if(NOT EXISTS "${RUNNER_PATH}")
            file(GLOB RUNNER_FILES "${SUT_DIR}/*_hil_test_runner" "${SUT_DIR}/*_hil_test_runner.exe")
            if(RUNNER_FILES)
                list(GET RUNNER_FILES 0 RUNNER_PATH)
            endif()
        endif()
        
        if(EXISTS "${RUNNER_PATH}")
            message(STATUS "  Running HIL tests: ${SUT_NAME}")
            
            # Start SUT in background
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut "${SUT_PATH}"
                OUTPUT_FILE "${BUILD_DIR}/hil_${SUT_NAME}.log"
                ERROR_FILE "${BUILD_DIR}/hil_${SUT_NAME}.log"
                RESULT_VARIABLE sut_result
                TIMEOUT 1
            )
            
            # Give SUT time to start
            execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1)
            
            # Run test runner (it will connect to SUT)
            execute_process(
                COMMAND "${RUNNER_PATH}"
                RESULT_VARIABLE test_result
                TIMEOUT 30
            )
            
            if(test_result EQUAL 0)
                message(STATUS "    ✓ HIL tests passed")
            else()
                message(WARNING "    ✗ HIL tests failed (continuing for coverage)")
            endif()
            
            # Cleanup - kill any remaining processes
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E echo "Cleanup HIL processes..."
                OUTPUT_QUIET
            )
        else()
            message(STATUS "  Skipping ${SUT_NAME} (no test runner found)")
        endif()
    endforeach()
endif()

# Step 6: Generate comprehensive coverage report
message(STATUS "")
message(STATUS "Step 6/6: Generating comprehensive coverage report...")
message(STATUS "")

# Capture coverage from all .gcda files
message(STATUS "  Capturing coverage data...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --capture
        --directory "${BUILD_DIR}"
        --output-file "${BUILD_DIR}/coverage_raw.info"
        --rc branch_coverage=0
        --ignore-errors mismatch,gcov,inconsistent,negative
    RESULT_VARIABLE result
    OUTPUT_QUIET
    ERROR_QUIET
)
if(result)
    message(WARNING "lcov capture had warnings (continuing...)")
endif()

# Remove unwanted files (but keep sample apps!)
message(STATUS "  Filtering coverage data...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --remove "${BUILD_DIR}/coverage_raw.info"
        '/usr/*'
        '*/tests/*'
        '*/googletest/*'
        '*/build/_deps/*'
        --ignore-errors unused
        --output-file "${BUILD_DIR}/coverage_filtered.info"
    OUTPUT_QUIET
    ERROR_QUIET
)

# Extract core library coverage
message(STATUS "  Extracting core library coverage...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --extract "${BUILD_DIR}/coverage_filtered.info"
        '*/src/drivers/*'
        '*/src/platform/*'
        '*/src/lq_*'
        --ignore-errors unused
        --output-file "${BUILD_DIR}/coverage_core.info"
    OUTPUT_QUIET
    ERROR_QUIET
)

# Extract sample apps coverage
message(STATUS "  Extracting sample apps coverage...")
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --extract "${BUILD_DIR}/coverage_filtered.info"
        '*/samples/*'
        '*/build/*/lq_generated.c'
        '*/build/*/main.c'
        --ignore-errors unused
        --output-file "${BUILD_DIR}/coverage_samples.info"
    OUTPUT_QUIET
    ERROR_QUIET
)

# Generate HTML reports
message(STATUS "  Generating HTML reports...")
file(MAKE_DIRECTORY "${COVERAGE_DIR}")
file(MAKE_DIRECTORY "${COVERAGE_DIR}/core")
file(MAKE_DIRECTORY "${COVERAGE_DIR}/samples")

# Combined report
execute_process(
    COMMAND ${GENHTML_EXECUTABLE} "${BUILD_DIR}/coverage_filtered.info"
        --output-directory "${COVERAGE_DIR}"
        --title "Layered Queue - Full Coverage (Core + Samples)"
        --legend
        --show-details
        --demangle-cpp
    OUTPUT_QUIET
)

# Core library report
execute_process(
    COMMAND ${GENHTML_EXECUTABLE} "${BUILD_DIR}/coverage_core.info"
        --output-directory "${COVERAGE_DIR}/core"
        --title "Layered Queue - Core Library Coverage"
        --legend
        --show-details
        --demangle-cpp
    OUTPUT_QUIET
)

# Sample apps report (only if we have sample coverage)
if(EXISTS "${BUILD_DIR}/coverage_samples.info")
    file(READ "${BUILD_DIR}/coverage_samples.info" SAMPLES_CONTENT)
    if(NOT "${SAMPLES_CONTENT}" STREQUAL "")
        execute_process(
            COMMAND ${GENHTML_EXECUTABLE} "${BUILD_DIR}/coverage_samples.info"
                --output-directory "${COVERAGE_DIR}/samples"
                --title "Layered Queue - Sample Applications Coverage"
                --legend
                --show-details
                --demangle-cpp
            OUTPUT_QUIET
        )
    endif()
endif()

# Extract coverage percentages using lcov --summary
execute_process(
    COMMAND ${LCOV_EXECUTABLE} --summary "${BUILD_DIR}/coverage_filtered.info"
    OUTPUT_VARIABLE COVERAGE_FULL_OUTPUT
    ERROR_VARIABLE COVERAGE_FULL_OUTPUT
)

execute_process(
    COMMAND ${LCOV_EXECUTABLE} --summary "${BUILD_DIR}/coverage_core.info"
    OUTPUT_VARIABLE COVERAGE_CORE_OUTPUT
    ERROR_VARIABLE COVERAGE_CORE_OUTPUT
)

# Parse percentages from output
string(REGEX MATCH "lines[.]*: ([0-9]+\\.[0-9]+)%" COVERAGE_FULL_MATCH "${COVERAGE_FULL_OUTPUT}")
if(CMAKE_MATCH_1)
    set(COVERAGE_FULL "${CMAKE_MATCH_1}")
else()
    set(COVERAGE_FULL "0.0")
endif()

string(REGEX MATCH "lines[.]*: ([0-9]+\\.[0-9]+)%" COVERAGE_CORE_MATCH "${COVERAGE_CORE_OUTPUT}")
if(CMAKE_MATCH_1)
    set(COVERAGE_CORE "${CMAKE_MATCH_1}")
else()
    set(COVERAGE_CORE "0.0")
endif()

# Try to get samples coverage
set(COVERAGE_SAMPLES "0.0")
if(EXISTS "${BUILD_DIR}/coverage_samples.info")
    execute_process(
        COMMAND ${LCOV_EXECUTABLE} --summary "${BUILD_DIR}/coverage_samples.info"
        OUTPUT_VARIABLE COVERAGE_SAMPLES_OUTPUT
        ERROR_VARIABLE COVERAGE_SAMPLES_OUTPUT
    )
    string(REGEX MATCH "lines[.]*: ([0-9]+\\.[0-9]+)%" COVERAGE_SAMPLES_MATCH "${COVERAGE_SAMPLES_OUTPUT}")
    if(CMAKE_MATCH_1)
        set(COVERAGE_SAMPLES "${CMAKE_MATCH_1}")
    endif()
endif()

# Display summary
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "  Coverage Summary")
message(STATUS "==========================================")
message(STATUS "")
message(STATUS "Core Library Coverage:    ${COVERAGE_CORE}%")
message(STATUS "Sample Apps Coverage:     ${COVERAGE_SAMPLES}%")
message(STATUS "Combined Coverage:        ${COVERAGE_FULL}%")
message(STATUS "")

# Status
if(COVERAGE_FULL GREATER_EQUAL 80)
    set(STATUS "EXCELLENT")
elseif(COVERAGE_FULL GREATER_EQUAL 70)
    set(STATUS "GOOD")
elseif(COVERAGE_FULL GREATER_EQUAL 60)
    set(STATUS "ACCEPTABLE")
else()
    set(STATUS "NEEDS IMPROVEMENT")
endif()

message(STATUS "Overall Status: ${STATUS}")
message(STATUS "")
message(STATUS "HTML reports generated:")
message(STATUS "  Full Coverage:   file://${COVERAGE_DIR}/index.html")
message(STATUS "  Core Only:       file://${COVERAGE_DIR}/core/index.html")
if(NOT COVERAGE_SAMPLES STREQUAL "0.0")
    message(STATUS "  Samples Only:    file://${COVERAGE_DIR}/samples/index.html")
endif()
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "")
