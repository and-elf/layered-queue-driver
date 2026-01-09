# LayeredQueue Application Generator
#
# Provides functions to create applications from device tree files
# with automatic code generation and EDS expansion.

#[=======================================================================[.rst:
LayeredQueueApp
---------------

Functions for creating LayeredQueue applications from device trees.

.. command:: add_lq_application

  Creates an executable from a device tree file with automatic code generation.

  ::

    add_lq_application(
      <target_name>
      DTS <dts_file>
      [EDS <eds_file>]
      [PLATFORM <platform>]
      [SOURCES <source>...]
    )

  ``<target_name>``
    Name of the executable target to create.

  ``DTS <dts_file>``
    Path to the device tree source file (relative to current source dir).

  ``EDS <eds_file>``
    Optional path to CANopen EDS file (enables EDS expansion).

  ``PLATFORM <platform>``
    Target platform: baremetal (default), freertos, zephyr, stm32, esp32, etc.

  ``SOURCES <source>...``
    Optional additional source files to link with the application.

  ``ENABLE_HIL_TESTS``
    Optional flag to generate HIL test infrastructure for this application.

Example::

  add_lq_application(motor_driver
    DTS motor_system.dts
    EDS example_motor.eds
    PLATFORM baremetal
  )

#]=======================================================================]

function(add_lq_application TARGET_NAME)
    cmake_parse_arguments(
        APP
        "ENABLE_HIL_TESTS"
        "DTS;EDS;PLATFORM"
        "SOURCES"
        ${ARGN}
    )

    if(NOT APP_DTS)
        message(FATAL_ERROR "add_lq_application: DTS file is required")
    endif()

    # Set defaults
    if(NOT APP_PLATFORM)
        set(APP_PLATFORM "baremetal")
    endif()

    # Resolve paths
    set(DTS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${APP_DTS}")
    if(APP_EDS)
        set(EDS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${APP_EDS}")
    endif()

    # Output directory
    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    # Determine if EDS expansion is needed
    if(APP_EDS)
        # Stage 1: Expand EDS references in DTS
        add_custom_command(
            OUTPUT ${GEN_DIR}/expanded.dts
                   ${GEN_DIR}/motor_signals.h
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                ${DTS_FILE}
                ${GEN_DIR}
                --expand-eds
                --signals-header ${GEN_DIR}/motor_signals.h
            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                    ${CMAKE_SOURCE_DIR}/scripts/canopen_eds_parser.py
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
    add_custom_command(
        OUTPUT ${GEN_DIR}/lq_generated.c
               ${GEN_DIR}/lq_generated.h
               ${GEN_DIR}/main.c
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${PARSED_DTS}
            ${GEN_DIR}
            --platform=${APP_PLATFORM}
        DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                ${PARSED_DTS}
        COMMENT "Generating ${TARGET_NAME} from device tree"
    )

    # Custom target to ensure generation happens in correct order
    add_custom_target(${TARGET_NAME}_codegen
        DEPENDS ${GEN_DIR}/lq_generated.c
                ${GEN_DIR}/lq_generated.h
                ${GEN_DIR}/main.c
                ${SIGNAL_HEADER}
    )

    # Create executable with generated code
    add_executable(${TARGET_NAME}
        ${GEN_DIR}/main.c
        ${GEN_DIR}/lq_generated.c
        ${APP_SOURCES}
    )

    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_codegen)

    # Include generated headers
    target_include_directories(${TARGET_NAME} PRIVATE ${GEN_DIR})

    # Link with layered_queue library
    target_link_libraries(${TARGET_NAME} PRIVATE layered_queue)

    # Print configuration
    message(STATUS "LayeredQueue App: ${TARGET_NAME}")
    message(STATUS "  DTS: ${APP_DTS}")
    if(APP_EDS)
        message(STATUS "  EDS: ${APP_EDS}")
    endif()
    message(STATUS "  Platform: ${APP_PLATFORM}")

    # Generate HIL test infrastructure if requested
    if(APP_ENABLE_HIL_TESTS)
        message(STATUS "  HIL Tests: Enabled")
        
        # Create HIL output directory
        set(HIL_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_hil")
        
        # Generate test DTS and test runner
        add_custom_command(
            OUTPUT ${HIL_DIR}/lq_generated_test.dts
                   ${HIL_DIR}/test_runner.c
            COMMAND ${CMAKE_COMMAND} -E make_directory ${HIL_DIR}
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                ${PARSED_DTS}
                ${HIL_DIR}/
            COMMAND ${CMAKE_SOURCE_DIR}/scripts/hil_test_gen.py
                ${HIL_DIR}/lq_generated_test.dts
                ${HIL_DIR}/
            DEPENDS ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
                    ${CMAKE_SOURCE_DIR}/scripts/hil_test_gen.py
                    ${PARSED_DTS}
            COMMENT "Generating HIL tests for ${TARGET_NAME}"
        )
        
        # Build HIL-enabled SUT binary
        add_executable(${TARGET_NAME}_hil_sut
            ${CMAKE_SOURCE_DIR}/tests/hil/real_sut.c
            ${GEN_DIR}/lq_generated.c
            # Core drivers needed for SUT
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_queue_core.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_util.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_engine.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hw_input.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil_platform.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_remap.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_scale.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_pid.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_verified_output.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_spi_source.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_j1939.c
            ${CMAKE_SOURCE_DIR}/src/platform/lq_platform_hil.c
        )
        
        add_dependencies(${TARGET_NAME}_hil_sut ${TARGET_NAME}_codegen)
        
        target_include_directories(${TARGET_NAME}_hil_sut PRIVATE
            ${GEN_DIR}
            ${CMAKE_SOURCE_DIR}/include
        )
        
        target_compile_definitions(${TARGET_NAME}_hil_sut PRIVATE
            LQ_PLATFORM_NATIVE=1
            FULL_APP=1
        )
        
        target_link_libraries(${TARGET_NAME}_hil_sut PRIVATE pthread)
        
        # Build test runner
        add_executable(${TARGET_NAME}_hil_test_runner
            ${HIL_DIR}/test_runner.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil.c
            ${CMAKE_SOURCE_DIR}/src/drivers/lq_hil_platform.c
        )
        
        add_custom_target(${TARGET_NAME}_hil_testgen
            DEPENDS ${HIL_DIR}/lq_generated_test.dts
                    ${HIL_DIR}/test_runner.c
        )
        
        add_dependencies(${TARGET_NAME}_hil_test_runner ${TARGET_NAME}_hil_testgen)
        
        target_include_directories(${TARGET_NAME}_hil_test_runner PRIVATE
            ${CMAKE_SOURCE_DIR}/include
        )
        
        target_link_libraries(${TARGET_NAME}_hil_test_runner PRIVATE pthread)
        
        # Create convenience target to run HIL tests
        add_custom_target(${TARGET_NAME}_hil_run
            COMMAND ${CMAKE_COMMAND} -E echo "=== Running HIL Tests for ${TARGET_NAME} ==="
            COMMAND ${CMAKE_COMMAND} -E env LQ_HIL_MODE=sut
                ${CMAKE_SOURCE_DIR}/tests/hil/run_hil_cmake.sh
                $<TARGET_FILE:${TARGET_NAME}_hil_sut>
                $<TARGET_FILE:${TARGET_NAME}_hil_test_runner>
            DEPENDS ${TARGET_NAME}_hil_sut ${TARGET_NAME}_hil_test_runner
            COMMENT "Running HIL tests for ${TARGET_NAME}"
        )
        
        message(STATUS "  HIL Targets: ${TARGET_NAME}_hil_sut, ${TARGET_NAME}_hil_test_runner, ${TARGET_NAME}_hil_run")
    endif()

endfunction()
