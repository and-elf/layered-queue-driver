# Zephyr module integration for layered-queue-driver
#
# This module provides two libraries:
# 1. Core driver library (platform-independent)
# 2. Zephyr platform implementation
#
# The libraries are automatically linked when CONFIG_LQ_DRIVER=y

if(CONFIG_LQ_DRIVER)
  zephyr_library_named(lq_driver)

  # Core driver sources (platform-independent)
  zephyr_library_sources(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_queue_core.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_engine.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_hw_input.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_remap.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_scale.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_pid.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_verified_output.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_util.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_gpio_pattern.c
  )

  # Zephyr platform implementation
  zephyr_library_sources(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_zephyr.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_can_zephyr.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_gpio_zephyr.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_pwm_zephyr.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_i2c_zephyr.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_uart_zephyr.c
  )

  # Public include directories (INTERFACE makes them available to app)
  zephyr_include_directories(
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
  )

  # Include app's generated headers (for lq_config.h with auto-calculated sizes)
  # Must happen after lq_driver library is defined
  zephyr_library_include_directories(${APPLICATION_BINARY_DIR}/app)
endif()
