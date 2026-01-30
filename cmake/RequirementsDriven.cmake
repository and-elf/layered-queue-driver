# Requirements-Driven Development Support for Layered Queue Driver
#
# This module adds support for generating DTS from requirements before
# building the application with add_lq_application().
#
# Workflow:
#   requirements/ → [reqgen.py] → app.dts → [dts_gen.py] → C code

#[=======================================================================[.rst:
add_lq_application_from_requirements
-------------------------------------

Generate DTS from requirements and build application.

This is a wrapper around add_lq_application() that first generates app.dts
from structured requirements files.

  ::

    add_lq_application_from_requirements(
      <target_name>
      REQUIREMENTS <requirements_dir>
      [DTS_OUTPUT <dts_file>]           # Default: ${CMAKE_CURRENT_BINARY_DIR}/app.dts
      [EDS <eds_file>]
      [PLATFORM <platform>]
      [RTOS <rtos>]
      [SOURCES <source>...]
      [ENABLE_HIL_TESTS]
      [STRICT]                          # Treat warnings as errors
      [AUTO_FIX]                        # Auto-resolve conflicts
      [FORCE]                           # Generate even with errors (not recommended)
    )

Requirements Structure::

  requirements/
    ├── high-level/          # Natural language markdown (HLRs)
    │   ├── speed-control.md
    │   └── safety-monitoring.md
    └── low-level/           # Structured YAML (LLRs)
        ├── llr-0.1-engine-config.yaml
        ├── llr-1.1-adc-sampling.yaml
        └── llr-1.2-speed-scaling.yaml

Example::

  add_lq_application_from_requirements(my_app
    REQUIREMENTS requirements/
    PLATFORM zephyr
    RTOS zephyr
  )

This will:
  1. Validate requirements (detect conflicts)
  2. Generate app.dts from requirements
  3. Call add_lq_application() with generated DTS
  4. Build application with west

#]=======================================================================]

function(add_lq_application_from_requirements TARGET_NAME)
    cmake_parse_arguments(
        APP
        "ENABLE_HIL_TESTS;STRICT;AUTO_FIX;FORCE"
        "REQUIREMENTS;DTS_OUTPUT;EDS;PLATFORM;RTOS"
        "SOURCES"
        ${ARGN}
    )

    if(NOT APP_REQUIREMENTS)
        message(FATAL_ERROR "add_lq_application_from_requirements: REQUIREMENTS directory is required")
    endif()

    # Resolve requirements directory path
    if(NOT IS_ABSOLUTE ${APP_REQUIREMENTS})
        set(REQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${APP_REQUIREMENTS}")
    else()
        set(REQ_DIR "${APP_REQUIREMENTS}")
    endif()

    if(NOT EXISTS ${REQ_DIR})
        message(FATAL_ERROR "Requirements directory not found: ${REQ_DIR}")
    endif()

    # Default output DTS file
    if(NOT APP_DTS_OUTPUT)
        set(APP_DTS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/app.dts")
    endif()

    # Determine script location. Use the module scripts under the repo to avoid
    # depending on the consumer project layout.
    set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/modules/layered-queue-driver/scripts")

    # Build reqgen.py command
    set(REQGEN_CMD ${SCRIPT_DIR}/reqgen.py ${REQ_DIR} generate-dts -o ${APP_DTS_OUTPUT})

    # Add optional flags
    if(APP_STRICT)
        list(APPEND REQGEN_CMD --strict)
    endif()
    if(APP_AUTO_FIX)
        list(APPEND REQGEN_CMD --auto-fix)
    endif()
    if(APP_FORCE)
        list(APPEND REQGEN_CMD --force)
    endif()

    # Find all requirement files for dependencies
    file(GLOB_RECURSE REQ_FILES
        ${REQ_DIR}/high-level/*.md
        ${REQ_DIR}/low-level/*.yaml
        ${REQ_DIR}/low-level/*.yml
    )

    # Build validation command
    set(VALIDATE_CMD python3 ${SCRIPT_DIR}/reqgen.py ${REQ_DIR} validate)
    if(APP_STRICT)
        list(APPEND VALIDATE_CMD --strict)
    endif()
    if(APP_AUTO_FIX)
        list(APPEND VALIDATE_CMD --auto-fix)
    endif()

    # Custom command to generate DTS from requirements
    add_custom_command(
        OUTPUT ${APP_DTS_OUTPUT}
        COMMAND ${CMAKE_COMMAND} -E echo "Validating requirements..."
        COMMAND ${VALIDATE_CMD}
        COMMAND ${CMAKE_COMMAND} -E echo "Generating DTS from requirements..."
        COMMAND python3 ${REQGEN_CMD}
        DEPENDS ${SCRIPT_DIR}/reqgen.py
                ${SCRIPT_DIR}/conflict_handler.py
                ${REQ_FILES}
        COMMENT "Generating ${APP_DTS_OUTPUT} from ${APP_REQUIREMENTS}"
        VERBATIM
    )

    # Custom target to ensure DTS generation
    add_custom_target(${TARGET_NAME}_reqgen
        DEPENDS ${APP_DTS_OUTPUT}
    )

    message(STATUS "=== Requirements-Driven Build ===")
    message(STATUS "Requirements: ${APP_REQUIREMENTS}")
    message(STATUS "Generated DTS: ${APP_DTS_OUTPUT}")
    if(APP_STRICT)
        message(STATUS "Mode: STRICT (warnings = errors)")
    elseif(APP_AUTO_FIX)
        message(STATUS "Mode: AUTO_FIX (auto-resolve conflicts)")
    else()
        message(STATUS "Mode: NORMAL (errors only)")
    endif()

    # Now call standard add_lq_application with generated DTS
    add_lq_application(${TARGET_NAME}
        DTS ${APP_DTS_OUTPUT}
        $<$<BOOL:${APP_EDS}>:EDS ${APP_EDS}>
        $<$<BOOL:${APP_PLATFORM}>:PLATFORM ${APP_PLATFORM}>
        $<$<BOOL:${APP_RTOS}>:RTOS ${APP_RTOS}>
        $<$<BOOL:${APP_SOURCES}>:SOURCES ${APP_SOURCES}>
        $<$<BOOL:${APP_ENABLE_HIL_TESTS}>:ENABLE_HIL_TESTS>
    )

    # Make application depend on requirements generation
    add_dependencies(${TARGET_NAME}_codegen ${TARGET_NAME}_reqgen)

    message(STATUS "===================================")
endfunction()


# Generate DTS and prj.conf from requirements at configure time and export paths
function(generate_requirements_prjconf)
    cmake_parse_arguments(
        GEN
        "FORCE"
        "REQUIREMENTS;DTS_OUTPUT;PRJCONF_OUTPUT;PLATFORM;RTOS"
        ""
        ${ARGN}
    )

    if(NOT GEN_REQUIREMENTS)
        message(FATAL_ERROR "generate_requirements_prjconf: REQUIREMENTS directory is required")
    endif()

    # Resolve requirements directory path
    if(NOT IS_ABSOLUTE ${GEN_REQUIREMENTS})
        set(REQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${GEN_REQUIREMENTS}")
    else()
        set(REQ_DIR "${GEN_REQUIREMENTS}")
    endif()

    if(NOT EXISTS ${REQ_DIR})
        message(FATAL_ERROR "Requirements directory not found: ${REQ_DIR}")
    endif()

    # Default output DTS file
    if(NOT GEN_DTS_OUTPUT)
        set(GEN_DTS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/prereq/app.dts")
    endif()

    # Default output prj.conf file
    if(NOT GEN_PRJCONF_OUTPUT)
        get_filename_component(_dts_dir "${GEN_DTS_OUTPUT}" DIRECTORY)
        set(GEN_PRJCONF_OUTPUT "${_dts_dir}/prj.conf")
    endif()

    # Determine script location: prefer the scripts directory next to this CMake module
    set(SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}/../scripts")
    if(NOT IS_ABSOLUTE "${SCRIPT_DIR}")
        get_filename_component(SCRIPT_DIR "${SCRIPT_DIR}" ABSOLUTE)
    endif()
    if(NOT EXISTS "${SCRIPT_DIR}/reqgen.py")
        set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
    endif()

    # Prefer common venv locations (module or repo .venv), then system python
    if(EXISTS "${CMAKE_SOURCE_DIR}/modules/.venv/bin/python3")
        set(PYTHON_EXEC "${CMAKE_SOURCE_DIR}/modules/.venv/bin/python3")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/modules/.venv/bin/python")
        set(PYTHON_EXEC "${CMAKE_SOURCE_DIR}/modules/.venv/bin/python")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
        set(PYTHON_EXEC "${CMAKE_SOURCE_DIR}/.venv/bin/python3")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/.venv/bin/python")
        set(PYTHON_EXEC "${CMAKE_SOURCE_DIR}/.venv/bin/python")
    else()
        find_program(PYTHON_EXEC python3 python)
    endif()

    if(NOT PYTHON_EXEC)
        message(FATAL_ERROR "No Python interpreter found to run requirement generators")
    endif()

    # Ensure output directory exists
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/prereq"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # At configure time we avoid running heavy generators. Create a minimal
    # prj.conf in the build/prereq directory so Zephyr's CMake can find a
    # configuration. Full generation runs at build time via the wrapper.
    set(_prj_out_dir "${CMAKE_CURRENT_BINARY_DIR}/prereq")
    set(GEN_PRJCONF_OUTPUT "${_prj_out_dir}/prj.conf")
    if(NOT EXISTS "${_prj_out_dir}")
        file(MAKE_DIRECTORY "${_prj_out_dir}")
    endif()
    if(NOT EXISTS ${GEN_PRJCONF_OUTPUT})
        file(WRITE ${GEN_PRJCONF_OUTPUT} "# Auto-generated minimal prj.conf - do not edit\n# Generated by layered-queue-driver (minimal at configure time)\n")
        message(DEBUG "Wrote minimal prj.conf to ${GEN_PRJCONF_OUTPUT}")
    endif()

    # Export variables to parent scope for use before find_package(Zephyr)
    set(GENERATED_DTS "${GEN_DTS_OUTPUT}" PARENT_SCOPE)
    set(GENERATED_PRJCONF "${GEN_PRJCONF_OUTPUT}" PARENT_SCOPE)
    set(CONF_FILE "${GEN_PRJCONF_OUTPUT}" PARENT_SCOPE)

    message(DEBUG "Generated DTS: ${GEN_DTS_OUTPUT}")
    message(DEBUG "Generated prj.conf: ${GEN_PRJCONF_OUTPUT}")
endfunction()


#[=======================================================================[.rst:
validate_requirements
----------------------

Standalone command to validate requirements without building.

  ::

    validate_requirements(
      REQUIREMENTS <requirements_dir>
      [STRICT]                          # Treat warnings as errors
      [AUTO_FIX]                        # Auto-resolve conflicts
    )

Returns non-zero exit code if validation fails (for CI/CD).

Example::

  validate_requirements(
    REQUIREMENTS requirements/
    STRICT
  )

#]=======================================================================]

function(validate_requirements)
    cmake_parse_arguments(
        VAL
        "STRICT;AUTO_FIX"
        "REQUIREMENTS"
        ""
        ${ARGN}
    )

    if(NOT VAL_REQUIREMENTS)
        message(FATAL_ERROR "validate_requirements: REQUIREMENTS directory is required")
    endif()

    # Resolve requirements directory path
    if(NOT IS_ABSOLUTE ${VAL_REQUIREMENTS})
        set(REQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${VAL_REQUIREMENTS}")
    else()
        set(REQ_DIR "${VAL_REQUIREMENTS}")
    endif()

    # Determine script location: prefer the scripts directory next to this CMake module
    set(SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}/../scripts")
    if(NOT IS_ABSOLUTE "${SCRIPT_DIR}")
        get_filename_component(SCRIPT_DIR "${SCRIPT_DIR}" ABSOLUTE)
    endif()
    if(NOT EXISTS "${SCRIPT_DIR}/reqgen.py")
        set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
    endif()

    # Build validation command
    set(VALIDATE_CMD python3 ${SCRIPT_DIR}/reqgen.py ${REQ_DIR} validate)
    if(VAL_STRICT)
        list(APPEND VALIDATE_CMD --strict)
    endif()
    if(VAL_AUTO_FIX)
        list(APPEND VALIDATE_CMD --auto-fix)
    endif()

    # Execute validation
    execute_process(
        COMMAND ${VALIDATE_CMD}
        RESULT_VARIABLE VALIDATION_RESULT
        OUTPUT_VARIABLE VALIDATION_OUTPUT
        ERROR_VARIABLE VALIDATION_ERROR
    )

    # Print output
    message(STATUS ${VALIDATION_OUTPUT})
    if(VALIDATION_ERROR)
        message(WARNING ${VALIDATION_ERROR})
    endif()

    # Check result
    if(NOT VALIDATION_RESULT EQUAL 0)
        message(FATAL_ERROR "Requirements validation failed! Fix conflicts before building.")
    else()
        message(STATUS "✅ Requirements validation passed")
    endif()
endfunction()


#[=======================================================================[.rst:
generate_requirements_docs
---------------------------

Generate requirements traceability documentation.

  ::

    generate_requirements_docs(
      REQUIREMENTS <requirements_dir>
      OUTPUT <output_file>              # Default: REQUIREMENTS.md
    )

Example::

  generate_requirements_docs(
    REQUIREMENTS requirements/
    OUTPUT REQUIREMENTS.md
  )

#]=======================================================================]

function(generate_requirements_docs)
    cmake_parse_arguments(
        DOC
        ""
        "REQUIREMENTS;OUTPUT"
        ""
        ${ARGN}
    )

    if(NOT DOC_REQUIREMENTS)
        message(FATAL_ERROR "generate_requirements_docs: REQUIREMENTS directory is required")
    endif()

    if(NOT DOC_OUTPUT)
        set(DOC_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/REQUIREMENTS.md")
    endif()

    # Resolve paths
    if(NOT IS_ABSOLUTE ${DOC_REQUIREMENTS})
        set(REQ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/${DOC_REQUIREMENTS}")
    else()
        set(REQ_DIR "${DOC_REQUIREMENTS}")
    endif()

    # Determine script location
    if(TARGET app AND DEFINED ZEPHYR_BASE)
        set(SCRIPT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/modules/layered-queue-driver/scripts")
    else()
        set(SCRIPT_DIR "${CMAKE_SOURCE_DIR}/scripts")
    endif()

    # Find all requirement files
    file(GLOB_RECURSE REQ_FILES
        ${REQ_DIR}/high-level/*.md
        ${REQ_DIR}/low-level/*.yaml
        ${REQ_DIR}/low-level/*.yml
    )

    # Generate documentation
    add_custom_command(
        OUTPUT ${DOC_OUTPUT}
        COMMAND python3 ${SCRIPT_DIR}/reqgen.py ${REQ_DIR} generate-docs -o ${DOC_OUTPUT}
        DEPENDS ${SCRIPT_DIR}/reqgen.py
                ${REQ_FILES}
        COMMENT "Generating requirements documentation: ${DOC_OUTPUT}"
    )

    add_custom_target(requirements_docs
        DEPENDS ${DOC_OUTPUT}
    )

    message(STATUS "Requirements documentation target: requirements_docs")
    message(STATUS "  Output: ${DOC_OUTPUT}")
endfunction()
