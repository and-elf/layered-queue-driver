cmake_minimum_required(VERSION 3.14)

project(layered_queue_driver VERSION 1.0.0 LANGUAGES C CXX)

# C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include LayeredQueue application generator
include(cmake/LayeredQueueApp.cmake)

# Options
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_HIL_TESTS "Enable HIL tests (requires SUT binary)" ON)

# Platform detection
if(NOT DEFINED LQ_PLATFORM)
    set(LQ_PLATFORM "native")
endif()

# Platform-specific source files
if(LQ_PLATFORM STREQUAL "freertos")
    set(PLATFORM_SOURCES src/platform/lq_platform_freertos.c)
    set(PLATFORM_DEFINITIONS LQ_PLATFORM_FREERTOS=1)
else()
    set(PLATFORM_SOURCES src/platform/lq_platform_native.c)
    set(PLATFORM_DEFINITIONS LQ_PLATFORM_NATIVE=1)
endif()

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Warning flags - store in variables to apply selectively
    set(WARNING_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Werror                      # Treat warnings as errors
        -Wshadow                     # Warn about variable shadowing
        -Wcast-align                 # Warn about pointer casts with alignment issues
        -Wunused                     # Warn about unused variables/functions
        -Wconversion                 # Warn about implicit type conversions
        -Wsign-conversion            # Warn about sign conversions
        -Wnull-dereference           # Warn about potential null dereferences
        -Wdouble-promotion           # Warn about float to double promotions
        -Wformat=2                   # Extra format string checks
        -Wimplicit-fallthrough       # Warn about switch fallthrough
        -Wnarrowing                  # Warn about narrowing conversions
    )
    
    # C-specific warnings
    set(C_WARNING_FLAGS
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wold-style-definition
    )
    
    # C++-specific warnings
    set(CXX_WARNING_FLAGS
        -Wnon-virtual-dtor
        -Woverloaded-virtual
    )
    
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# Core library
add_library(layered_queue
    src/drivers/lq_queue_core.c
    src/drivers/lq_util.c
    src/drivers/lq_spi_source.c
    src/drivers/lq_engine.c
    src/drivers/lq_hw_input.c
    src/drivers/lq_j1939.c
    src/drivers/lq_canopen.c
    src/drivers/lq_uds.c
    src/drivers/lq_isotp.c
    src/drivers/lq_uds_can.c
    src/drivers/lq_config.c
    src/drivers/lq_hil.c
    src/drivers/lq_hil_platform.c
    src/drivers/lq_remap.c
    src/drivers/lq_scale.c
    src/drivers/lq_verified_output.c
    src/drivers/lq_dtc.c
    src/drivers/lq_pid.c
    src/drivers/lq_bldc.c
    src/platform/lq_platform_stubs.c
    ${PLATFORM_SOURCES}
)

target_include_directories(layered_queue PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(layered_queue PUBLIC
    ${PLATFORM_DEFINITIONS}
)

# Apply strict warning flags to our library only
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(layered_queue PRIVATE
        ${WARNING_FLAGS}
        $<$<COMPILE_LANGUAGE:C>:${C_WARNING_FLAGS}>
        $<$<COMPILE_LANGUAGE:CXX>:${CXX_WARNING_FLAGS}>
    )
endif()

# Platform-specific linking
if(LQ_PLATFORM STREQUAL "freertos")
    # Link FreeRTOS if available
    if(TARGET freertos_kernel)
        target_link_libraries(layered_queue PUBLIC freertos_kernel)
    endif()
else()
    # Link pthread for native platform
    find_package(Threads REQUIRED)
    target_link_libraries(layered_queue PUBLIC Threads::Threads)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    
    # Fetch Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    # Generate automotive sample code for tests (not the test file itself)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/lq_generated.c
               ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/lq_generated.h
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
            ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
        COMMENT "Generating automotive sample code for tests"
    )
    
    # Source files for all_tests
    set(ALL_TESTS_SOURCES
        tests/queue_test.cpp
        tests/driver_test.cpp
        tests/dtc_test.cpp
        tests/j1939_test.cpp
        tests/canopen_test.cpp
        tests/uds_test.cpp
        tests/isotp_test.cpp
        tests/config_test.cpp
        tests/engine_test.cpp
        tests/hw_input_test.cpp
        tests/hil_platform_test.cpp
        tests/bldc_test.cpp
        samples/automotive/src/lq_generated_test.cpp  # Handwritten integration tests
        ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/lq_generated.c  # Generated code under test
        src/drivers/lq_hil.c
        src/drivers/lq_hil_platform.c
        src/drivers/lq_j1939.c
        src/drivers/lq_canopen.c
        src/drivers/lq_uds.c
        src/drivers/lq_isotp.c
        src/drivers/lq_uds_can.c
        src/drivers/lq_config.c
    )
    
    # Add HIL tests if enabled
    if(ENABLE_HIL_TESTS)
        list(APPEND ALL_TESTS_SOURCES tests/hil_test.cpp)
    endif()
    
    # Combined test executable with all tests (unit + HIL)
    add_executable(all_tests ${ALL_TESTS_SOURCES})
    
    target_link_libraries(all_tests PRIVATE
        layered_queue
        GTest::gtest_main  # Single main for all tests
    )
    
    target_include_directories(all_tests PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated
    )
    
    # Apply strict warning flags to test code
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(all_tests PRIVATE
            ${WARNING_FLAGS}
            $<$<COMPILE_LANGUAGE:C>:${C_WARNING_FLAGS}>
            $<$<COMPILE_LANGUAGE:CXX>:${CXX_WARNING_FLAGS}>
            # Allow missing field initializers in tests for brevity
            -Wno-missing-field-initializers
        )
    endif()
    
    # HIL tests need the SUT binary to be available
    if(ENABLE_HIL_TESTS)
        add_dependencies(all_tests hil-sut-full)
    endif()
    
    include(GoogleTest)
    gtest_discover_tests(all_tests)
    
    # Add custom target for running tests
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS all_tests
        COMMENT "Running all tests..."
    )
    
    # HIL testing targets
    
    # Simple echo SUT (for framework testing)
    add_custom_target(hil-sut-simple
        COMMAND ${CMAKE_COMMAND} -E echo "Building simple echo SUT..."
        COMMAND ${CMAKE_C_COMPILER} 
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/simple_sut.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_sut_simple
        COMMENT "Building simple HIL SUT"
    )
    
    # HIL Platform demo (shows how platform calls are intercepted)
    add_custom_target(hil-sut
        COMMAND ${CMAKE_COMMAND} -E echo "Building HIL platform demo..."
        COMMAND ${CMAKE_C_COMPILER} 
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/real_sut.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/lq_platform_hil.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
        COMMENT "Building HIL platform demo"
    )
    
    # Real SUT with full generated application
    add_custom_target(hil-sut-full
        COMMAND ${CMAKE_COMMAND} -E echo "Building FULL SUT with generated code..."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/hil
        # Generate SUT code from DTS
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
            ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/
        # Build SUT binary
        COMMAND ${CMAKE_C_COMPILER} 
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/real_sut.c
            ${CMAKE_CURRENT_BINARY_DIR}/automotive_generated/lq_generated.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_queue_core.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_util.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_spi_source.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_engine.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hw_input.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_j1939.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil_platform.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_remap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_scale.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_pid.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_verified_output.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/lq_platform_hil.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -I${CMAKE_CURRENT_BINARY_DIR}/automotive_generated
            -DLQ_PLATFORM_NATIVE=1
            -DFULL_APP=1
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_sut_full
        # Auto-generate test cases from same DTS
        COMMAND ${CMAKE_COMMAND} -E echo "Generating HIL tests for SUT..."
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
            ${CMAKE_CURRENT_BINARY_DIR}/hil/
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/hil_test_gen.py
            ${CMAKE_CURRENT_BINARY_DIR}/hil/lq_generated_test.dts
            ${CMAKE_CURRENT_BINARY_DIR}/hil/
        # Build test runner
        COMMAND ${CMAKE_C_COMPILER}
            ${CMAKE_CURRENT_BINARY_DIR}/hil/test_runner.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_hil_platform.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/drivers/lq_j1939.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner
        COMMENT "Building full HIL SUT and auto-generating tests"
    )
    
    # Legacy target for manual test generation (now handled by hil-sut-full)
    add_custom_target(hil-tests
        DEPENDS hil-sut-full
        COMMENT "HIL tests are now auto-generated with hil-sut-full"
    )
    
    add_custom_target(hil
        COMMAND ${CMAKE_COMMAND} -E echo "=== Running HIL Tests ==="
        COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut
            ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/run_hil_cmake.sh
            ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
            ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner
        DEPENDS hil-sut-full
        COMMENT "Running HIL test suite"
    )
    
    # Quick HIL test (doesn't fail build on test failure)
    add_custom_target(hil-quick
        COMMAND ${CMAKE_COMMAND} -E echo "=== Quick HIL Test ==="
        COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/run_hil_cmake.sh
            ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
            ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner || true
        DEPENDS hil-sut-full
        COMMENT "Running HIL tests (non-failing)"
    )
endif()

# ============================================================================
# Arduino examples compilation test
# ============================================================================

# Function to add Arduino compilation targets for a specific board
function(add_arduino_examples_for_board BOARD_FQBN BOARD_NAME)
    # Get list of Arduino example directories
    file(GLOB ARDUINO_EXAMPLES 
        ${CMAKE_CURRENT_SOURCE_DIR}/arduino/examples/*/*.ino
    )
    
    # Create test target for each example
    foreach(EXAMPLE_PATH ${ARDUINO_EXAMPLES})
        get_filename_component(EXAMPLE_DIR ${EXAMPLE_PATH} DIRECTORY)
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)
        
        # Skip platform-specific examples for wrong board
        set(SKIP_EXAMPLE FALSE)
        if(${BOARD_NAME} STREQUAL "avr")
            # AVR can't build ESP32/SAMD specific examples
            if(${EXAMPLE_NAME} MATCHES "ESP32" OR ${EXAMPLE_NAME} MATCHES "SAMD")
                set(SKIP_EXAMPLE TRUE)
            endif()
        elseif(${BOARD_NAME} STREQUAL "esp32")
            # ESP32 can't build SAMD specific examples
            if(${EXAMPLE_NAME} MATCHES "SAMD")
                set(SKIP_EXAMPLE TRUE)
            endif()
        elseif(${BOARD_NAME} MATCHES "samd")
            # SAMD can't build ESP32 specific examples
            if(${EXAMPLE_NAME} MATCHES "ESP32")
                set(SKIP_EXAMPLE TRUE)
            endif()
        endif()
        
        if(NOT SKIP_EXAMPLE)
            # Create unique target name with board identifier
            set(TARGET_NAME arduino-${BOARD_NAME}-${EXAMPLE_NAME})
            
            add_custom_target(${TARGET_NAME}
                COMMAND ${ARDUINO_CLI} compile 
                    --fqbn ${BOARD_FQBN}
                    --library ${CMAKE_CURRENT_SOURCE_DIR}/arduino
                    --build-property "compiler.cpp.extra_flags=-I${CMAKE_CURRENT_SOURCE_DIR}/include"
                    ${EXAMPLE_DIR}
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Compiling ${EXAMPLE_NAME} for ${BOARD_NAME}"
            )
            
            # Add to board-specific meta-target
            set(BOARD_TARGET arduino-${BOARD_NAME})
            if(NOT TARGET ${BOARD_TARGET})
                add_custom_target(${BOARD_TARGET})
            endif()
            add_dependencies(${BOARD_TARGET} ${TARGET_NAME})
            
            # Add to global arduino-check target
            if(NOT TARGET arduino-check)
                add_custom_target(arduino-check)
            endif()
            add_dependencies(arduino-check ${TARGET_NAME})
        endif()
    endforeach()
    
    message(STATUS "  ${BOARD_NAME}: make arduino-${BOARD_NAME}")
endfunction()

# Check for arduino-cli and add compilation targets
find_program(ARDUINO_CLI arduino-cli)
if(ARDUINO_CLI)
    message(STATUS "Found arduino-cli: ${ARDUINO_CLI}")
    message(STATUS "Arduino compilation targets:")
    
    # Check which cores are installed
    execute_process(
        COMMAND ${ARDUINO_CLI} core list
        OUTPUT_VARIABLE ARDUINO_CORES
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    
    # NOTE: AVR boards (Uno, Mega, etc.) use reduced configuration (lq_config_avr.h)
    # Default configuration requires >2KB RAM which exceeds Uno's 2KB limit
    # AVR uses smaller limits: 8 signals, 4 scales, 2 PIDs, etc.
    # This fits in ~1.8KB RAM on Uno, or ~2-3KB on Mega with room to grow
    
    # Add targets for each installed core
    string(FIND "${ARDUINO_CORES}" "arduino:avr" AVR_FOUND)
    if(AVR_FOUND GREATER -1)
        add_arduino_examples_for_board("arduino:avr:uno" "avr-uno")
        add_arduino_examples_for_board("arduino:avr:mega" "avr-mega")
    endif()
    
    # Add targets for each installed 32-bit core
    string(FIND "${ARDUINO_CORES}" "arduino:samd" SAMD_FOUND)
    if(SAMD_FOUND GREATER -1)
        add_arduino_examples_for_board("arduino:samd:mkrzero" "samd21")
        add_arduino_examples_for_board("arduino:samd:adafruit_metro_m4" "samd51")
    endif()
    
    string(FIND "${ARDUINO_CORES}" "esp32:esp32" ESP32_FOUND)
    if(ESP32_FOUND GREATER -1)
        add_arduino_examples_for_board("esp32:esp32:esp32" "esp32")
    endif()
    
    string(FIND "${ARDUINO_CORES}" "STMicroelectronics:stm32" STM32_FOUND)
    if(STM32_FOUND GREATER -1)
        add_arduino_examples_for_board("STMicroelectronics:stm32:Nucleo_64" "stm32")
    endif()
    
    if(TARGET arduino-check)
        message(STATUS "  all: make arduino-check")
        message(STATUS "")
        message(STATUS "Install cores with:")
        if(NOT AVR_FOUND GREATER -1)
            message(STATUS "  arduino-cli core install arduino:avr")
        endif()
        if(NOT SAMD_FOUND GREATER -1)
            message(STATUS "  arduino-cli core install arduino:samd")
        endif()
        if(NOT ESP32_FOUND GREATER -1)
            message(STATUS "  arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json")
        endif()
        if(NOT STM32_FOUND GREATER -1)
            message(STATUS "  arduino-cli core install STMicroelectronics:stm32 --additional-urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json")
        endif()
        message(STATUS "")
        message(STATUS "Note: AVR uses reduced config (~1.8KB RAM), 32-bit MCUs use full config")
    else()
        message(STATUS "  No Arduino cores installed")
        message(STATUS "  Install AVR: arduino-cli core install arduino:avr")
    endif()
else()
    message(STATUS "arduino-cli not found - skipping Arduino compilation tests")
    message(STATUS "Install: curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh")
endif()

# Install
install(TARGETS layered_queue
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Layered Queue Driver Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Platform: ${LQ_PLATFORM}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Enable coverage: ${ENABLE_COVERAGE}")
if(BUILD_TESTS)
    message(STATUS "Enable HIL tests: ${ENABLE_HIL_TESTS}")
endif()
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(BUILD_TESTS)
    message(STATUS "")
    message(STATUS "Available test targets:")
    message(STATUS "  make test         - Run unit tests")
    message(STATUS "  make hil-quick    - Run HIL tests (demo)")
    message(STATUS "  make hil          - Run HIL tests (strict)")
    if(ARDUINO_CLI)
        message(STATUS "  make arduino-check - Compile all Arduino examples")
    endif()
endif()
message(STATUS "==========================================")
message(STATUS "")

# CANopen sample
if(BUILD_TESTS)
    add_subdirectory(samples/canopen)
    add_subdirectory(samples/automotive)
    add_subdirectory(samples/basic)
endif()
