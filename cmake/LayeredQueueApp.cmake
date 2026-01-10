# Enhanced add_lq_application that works with both standalone and Zephyr environments
#
# This is a drop-in replacement for layered-queue-driver's add_lq_application
# that automatically detects Zephyr and adapts accordingly.
#
# Usage: Just use add_lq_application() as normal - it auto-detects!

#[=======================================================================[.rst:
add_lq_application (Zephyr-enhanced)
-------------------------------------

Enhanced version that automatically detects and adapts to Zephyr environment.

Works both standalone and within Zephyr - same API as the original!

  ::

    add_lq_application(
      <target_name>
      DTS <dts_file>
      [EDS <eds_file>]
      [PLATFORM <platform>]
      [RTOS <rtos>]
      [SOURCES <source>...]
      [ENABLE_HIL_TESTS]
    )

  In Zephyr environment:
    - Automatically uses 'app' target instead of creating new executable
    - TARGET_NAME is used only for code generation namespacing
    - PLATFORM defaults to "zephyr"
  
  In standalone environment:
    - Creates executable as normal
    - Full original functionality

Example (works in both contexts)::

  add_lq_application(my_app
    DTS app.dts
    PLATFORM zephyr
  )

#]=======================================================================]

function(add_lq_application TARGET_NAME)
    cmake_parse_arguments(
        APP
        "ENABLE_HIL_TESTS"
        "DTS;EDS;PLATFORM;RTOS"
        "SOURCES"
        ${ARGN}
    )

    if(NOT APP_DTS)
        message(FATAL_ERROR "add_lq_application: DTS file is required")
    endif()

    # Detect if we're in a Zephyr environment
    if(TARGET app AND DEFINED ZEPHYR_BASE)
        set(IS_ZEPHYR TRUE)
        message(STATUS "Detected Zephyr environment - adapting build")
    else()
        set(IS_ZEPHYR FALSE)
    endif()

    # Set defaults
    if(NOT APP_PLATFORM)
        if(IS_ZEPHYR)
            set(APP_PLATFORM "zephyr")
        else()
            set(APP_PLATFORM "native")
        endif()
    endif()
    
    if(NOT APP_RTOS)
        if(IS_ZEPHYR)
            set(APP_RTOS "zephyr")
        else()
            set(APP_RTOS "baremetal")
        endif()
    endif()

    # Resolve DTS file path
    if(NOT IS_ABSOLUTE ${APP_DTS})
        set(DTS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${APP_DTS}")
    else()
        set(DTS_FILE "${APP_DTS}")
    endif()

    # Resolve EDS file path if provided
    if(APP_EDS)
        if(NOT IS_ABSOLUTE ${APP_EDS})
            set(EDS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${APP_EDS}")
        else()
            set(EDS_FILE "${APP_EDS}")
        endif()
    endif()

    # Determine script location based on environment
    if(IS_ZEPHYR)
        set(SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/layered-queue-driver/scripts")
    else()
        set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
    endif()

    # Output directory for generated files
    if(IS_ZEPHYR)
        set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
    else()
        set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}")
    endif()

    # Stage 1: EDS expansion (if EDS file provided)
    if(APP_EDS)
        add_custom_command(
            OUTPUT ${GEN_DIR}/expanded.dts
                   ${GEN_DIR}/motor_signals.h
            COMMAND ${SCRIPT_DIR}/dts_gen.py
                ${DTS_FILE}
                ${GEN_DIR}
                --expand-eds
                --signals-header ${GEN_DIR}/motor_signals.h
            DEPENDS ${SCRIPT_DIR}/dts_gen.py
                    ${DTS_FILE}
                    ${EDS_FILE}
            COMMENT "Expanding EDS references in ${APP_DTS}"
        )
        set(PARSED_DTS "${GEN_DIR}/expanded.dts")
        set(SIGNAL_HEADER "${GEN_DIR}/motor_signals.h")
    else()
        # No EDS expansion needed
        set(PARSED_DTS "${DTS_FILE}")
        set(SIGNAL_HEADER "")
    endif()

    # Stage 2: Generate lq_generated.c/h and main.c from DTS
    if(SIGNAL_HEADER)
        set(GEN_OUTPUTS
            ${GEN_DIR}/lq_generated.c
            ${GEN_DIR}/lq_generated.h
            ${GEN_DIR}/main.c
            ${SIGNAL_HEADER}
        )
        set(GEN_DEPENDS
            ${SCRIPT_DIR}/dts_gen.py
            ${PARSED_DTS}
            ${SIGNAL_HEADER}
        )
    else()
        set(GEN_OUTPUTS
            ${GEN_DIR}/lq_generated.c
            ${GEN_DIR}/lq_generated.h
            ${GEN_DIR}/main.c
        )
        set(GEN_DEPENDS
            ${SCRIPT_DIR}/dts_gen.py
            ${PARSED_DTS}
        )
    endif()

    add_custom_command(
        OUTPUT ${GEN_OUTPUTS}
        COMMAND ${SCRIPT_DIR}/dts_gen.py
            ${PARSED_DTS}
            ${GEN_DIR}
            --platform=${APP_PLATFORM}
        DEPENDS ${GEN_DEPENDS}
        COMMENT "Generating code from device tree for ${TARGET_NAME}"
    )

    # Custom target to ensure generation happens
    add_custom_target(${TARGET_NAME}_codegen
        DEPENDS ${GEN_OUTPUTS}
    )

    # Branch based on environment
    if(IS_ZEPHYR)
        # Zephyr mode: add sources to existing 'app' target
        target_sources(app PRIVATE
            ${GEN_DIR}/main.c
            ${GEN_DIR}/lq_generated.c
            ${APP_SOURCES}
        )
        
        target_include_directories(app PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/modules/layered-queue-driver/include
            ${GEN_DIR}
        )
        
        add_dependencies(app ${TARGET_NAME}_codegen)
        
        message(STATUS "LayeredQueue App (Zephyr): ${TARGET_NAME}")
        message(STATUS "  DTS: ${APP_DTS}")
        if(APP_EDS)
            message(STATUS "  EDS: ${APP_EDS}")
        endif()
        message(STATUS "  Platform: ${APP_PLATFORM}")
        message(STATUS "  Target: app (Zephyr)")
    else()
        # Standalone mode: create new executable
        add_executable(${TARGET_NAME}
            ${GEN_DIR}/main.c
            ${GEN_DIR}/lq_generated.c
            ${APP_SOURCES}
        )
        
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_codegen)
        
        # Set output directory
        set_target_properties(${TARGET_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
        )
        
        target_include_directories(${TARGET_NAME} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${GEN_DIR}
        )
        
        # Link with layered_queue library if available
        if(TARGET layered_queue)
            target_link_libraries(${TARGET_NAME} PRIVATE layered_queue)
        endif()
        
        # Platform-specific pthread linking
        if(APP_PLATFORM STREQUAL "native" OR APP_RTOS STREQUAL "baremetal")
            find_package(Threads)
            if(Threads_FOUND)
                target_link_libraries(${TARGET_NAME} PRIVATE Threads::Threads)
            endif()
        endif()
        
        message(STATUS "LayeredQueue App (Standalone): ${TARGET_NAME}")
        message(STATUS "  DTS: ${APP_DTS}")
        if(APP_EDS)
            message(STATUS "  EDS: ${APP_EDS}")
        endif()
        message(STATUS "  Platform: ${APP_PLATFORM}")
        message(STATUS "  RTOS: ${APP_RTOS}")
        message(STATUS "  Target: ${TARGET_NAME} (executable)")
    endif()
    
    # HIL Tests (comprehensive test generation for 90-100% coverage)
    if(APP_ENABLE_HIL_TESTS AND NOT IS_ZEPHYR)
        message(STATUS "  HIL Tests: Enabled - generating comprehensive coverage tests")
        
        # Output directory for HIL test artifacts
        set(HIL_DIR "${GEN_DIR}/hil")
        
        # Generate comprehensive HIL tests from DTS
        add_custom_command(
            OUTPUT ${HIL_DIR}/comprehensive_hil_tests.dts
                   ${HIL_DIR}/test_runner.cpp
                   ${HIL_DIR}/${TARGET_NAME}_hil_sut
            COMMAND ${CMAKE_COMMAND} -E make_directory ${HIL_DIR}
            # Step 1: Generate comprehensive HIL test DTS
            COMMAND ${SCRIPT_DIR}/generate_comprehensive_hil_tests.py
                ${PARSED_DTS}
                ${HIL_DIR}
            # Step 2: Generate C test runner from test DTS
            COMMAND ${SCRIPT_DIR}/hil_test_gen.py
                ${HIL_DIR}/comprehensive_hil_tests.dts
                ${HIL_DIR}
            DEPENDS ${GEN_OUTPUTS}
                    ${SCRIPT_DIR}/generate_comprehensive_hil_tests.py
                    ${SCRIPT_DIR}/hil_test_gen.py
            COMMENT "Generating comprehensive HIL tests for ${TARGET_NAME}"
        )
        
        # Build SUT binary with coverage (as executable target)
        add_executable(${TARGET_NAME}_hil_sut
            ${GEN_DIR}/main.c
            ${GEN_DIR}/lq_generated.c
        )
        
        target_include_directories(${TARGET_NAME}_hil_sut PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${GEN_DIR}
        )
        
        target_compile_definitions(${TARGET_NAME}_hil_sut PRIVATE
            LQ_PLATFORM_NATIVE=1
        )
        
        target_link_libraries(${TARGET_NAME}_hil_sut PRIVATE
            layered_queue
            Threads::Threads
        )
        
        if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR ENABLE_COVERAGE)
            target_compile_options(${TARGET_NAME}_hil_sut PRIVATE --coverage)
            target_link_options(${TARGET_NAME}_hil_sut PRIVATE --coverage)
        endif()
        
        set_target_properties(${TARGET_NAME}_hil_sut PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${HIL_DIR}"
        )
        
        add_dependencies(${TARGET_NAME}_hil_sut ${TARGET_NAME}_codegen)
        
        # Build HIL test runner executable
        add_executable(${TARGET_NAME}_hil_test_runner
            ${HIL_DIR}/test_runner.cpp
        )
        
        # Ensure test runner source is generated first
        set_source_files_properties(${HIL_DIR}/test_runner.cpp PROPERTIES GENERATED TRUE)
        
        target_sources(${TARGET_NAME}_hil_test_runner PRIVATE
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil_platform.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_j1939.c
        )
        
        target_include_directories(${TARGET_NAME}_hil_test_runner PRIVATE
            ${CMAKE_SOURCE_DIR}/include
        )
        
        target_link_libraries(${TARGET_NAME}_hil_test_runner PRIVATE
            Threads::Threads
            GTest::gtest_main
        )
        
        set_target_properties(${TARGET_NAME}_hil_test_runner PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${HIL_DIR}"
        )
        
        add_custom_target(${TARGET_NAME}_hil_tests_dts
            DEPENDS ${HIL_DIR}/comprehensive_hil_tests.dts
                    ${HIL_DIR}/test_runner.cpp
        )
        
        add_dependencies(${TARGET_NAME}_hil_test_runner ${TARGET_NAME}_hil_tests_dts)
        add_dependencies(${TARGET_NAME}_hil_sut layered_queue ${TARGET_NAME}_hil_tests_dts)
        
        # Add dependency to main target
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_hil_sut ${TARGET_NAME}_hil_test_runner)
        
        message(STATUS "    Comprehensive HIL tests will be generated")
        message(STATUS "    Expected coverage: 90-100% of generated code")
    elseif(APP_ENABLE_HIL_TESTS AND IS_ZEPHYR)
        message(STATUS "  HIL Tests: Not supported in Zephyr mode")
    endif()

endfunction()
