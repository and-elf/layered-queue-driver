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
        add_compile_options(--coverage)
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
    
    # Queue tests
    add_executable(queue_test
        tests/queue_test.cpp
    )
    
    target_link_libraries(queue_test PRIVATE
        layered_queue
        GTest::gtest
        GTest::gtest_main
    )
    
    include(GoogleTest)
    gtest_discover_tests(queue_test)
    
    # Generated system tests (from DTS)
    add_executable(generated_test
        samples/automotive/src/lq_generated_test.cpp
        samples/automotive/src/lq_generated.c
    )
    
    target_link_libraries(generated_test PRIVATE
        layered_queue
        GTest::gtest
        GTest::gtest_main
    )
    
    target_include_directories(generated_test PRIVATE
        samples/automotive/src
    )
    
    gtest_discover_tests(generated_test)
    
    # Add custom target for running tests
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS queue_test generated_test
        COMMENT "Running tests..."
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
message(STATUS "==========================================")
message(STATUS "")
