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
        ""
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

endfunction()
