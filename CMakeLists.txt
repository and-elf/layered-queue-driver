cmake_minimum_required(VERSION 3.14)

project(layered_queue_driver VERSION 1.0.0 LANGUAGES C CXX)

# C/C++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_TESTS "Build tests" ON)
option(ENABLE_COVERAGE "Enable code coverage" OFF)

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
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(ENABLE_COVERAGE)
        add_compile_options(--coverage -O0 -g)
        add_link_options(--coverage)
    endif()
endif()

# Core library
add_library(layered_queue
    src/lq_queue_core.c
    src/lq_util.c
    src/lq_spi_source.c
    src/lq_engine.c
    src/lq_hw_input.c
    src/lq_j1939.c
    src/lq_hil.c
    src/lq_remap.c
    src/lq_scale.c
    src/lq_verified_output.c
    src/lq_pid.c
    ${PLATFORM_SOURCES}
)

target_include_directories(layered_queue PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_definitions(layered_queue PUBLIC
    ${PLATFORM_DEFINITIONS}
)

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
    
    # Combined test executable with all tests (unit + HIL)
    add_executable(all_tests
        tests/queue_test.cpp
        tests/driver_test.cpp
        tests/hil_test.cpp
        samples/automotive/src/lq_generated_test.cpp
        samples/automotive/src/lq_generated.c
        src/lq_hil.c
        src/lq_j1939.c
    )
    
    target_link_libraries(all_tests PRIVATE
        layered_queue
        GTest::gtest_main  # Single main for all tests
    )
    
    target_include_directories(all_tests PRIVATE
        samples/automotive/src
    )
    
    # HIL tests need the SUT binary to be available
    add_dependencies(all_tests hil-sut-full)
    
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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_hil.c
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
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/lq_platform_hil.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
        COMMENT "Building HIL platform demo"
    )
    
    # Real SUT with full generated application (when code is up to date)
    add_custom_target(hil-sut-full
        COMMAND ${CMAKE_COMMAND} -E echo "Building FULL SUT with generated code..."
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/src/
        COMMAND ${CMAKE_C_COMPILER} 
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/real_sut.c
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/src/lq_generated.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_queue_core.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_util.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_spi_source.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_engine.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_hw_input.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_j1939.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_remap.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_scale.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_pid.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_verified_output.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/lq_platform_hil.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -I${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/src
            -DLQ_PLATFORM_NATIVE=1
            -DFULL_APP=1
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_sut_full
        COMMENT "Building full HIL SUT with real generated code"
    )
    
    add_custom_target(hil-tests
        COMMAND ${CMAKE_COMMAND} -E echo "Generating HIL test DTS..."
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/samples/automotive/app.dts
            ${CMAKE_CURRENT_BINARY_DIR}/hil/
        COMMAND ${CMAKE_COMMAND} -E echo "Generating test runner..."
        COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/hil_test_gen.py
            ${CMAKE_CURRENT_BINARY_DIR}/hil/lq_generated_test.dts
            ${CMAKE_CURRENT_BINARY_DIR}/hil/
        COMMAND ${CMAKE_COMMAND} -E echo "Compiling test runner..."
        COMMAND ${CMAKE_C_COMPILER}
            ${CMAKE_CURRENT_BINARY_DIR}/hil/test_runner.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_hil.c
            ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_j1939.c
            -I${CMAKE_CURRENT_SOURCE_DIR}/include
            -lpthread
            -o ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner
        DEPENDS hil-sut
        COMMENT "Generating and compiling HIL tests"
    )
    
    add_custom_target(hil
        COMMAND ${CMAKE_COMMAND} -E echo "=== Running HIL Tests ==="
        COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut
            ${CMAKE_COMMAND} -E env PYTHONUNBUFFERED=1
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/run_hil_cmake.sh
            ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
            ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner
        DEPENDS hil-tests
        COMMENT "Running HIL test suite"
    )
    
    # Quick HIL test (doesn't fail build on test failure)
    add_custom_target(hil-quick
        COMMAND ${CMAKE_COMMAND} -E echo "=== Quick HIL Test ==="
        COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/hil/run_hil_cmake.sh
            ${CMAKE_CURRENT_BINARY_DIR}/hil_sut
            ${CMAKE_CURRENT_BINARY_DIR}/hil_test_runner || true
        DEPENDS hil-tests
        COMMENT "Running HIL tests (non-failing)"
    )
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
message(STATUS "C Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
if(BUILD_TESTS)
    message(STATUS "")
    message(STATUS "Available test targets:")
    message(STATUS "  make test       - Run unit tests")
    message(STATUS "  make hil-quick  - Run HIL tests (demo)")
    message(STATUS "  make hil        - Run HIL tests (strict)")
endif()
message(STATUS "==========================================")
message(STATUS "")
