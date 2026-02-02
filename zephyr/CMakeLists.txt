# Zephyr module integration for layered-queue-driver
#
# This module provides two libraries:
# 1. Core driver library (platform-independent)
# 2. Zephyr platform implementation
# 3. Zephyr device drivers (DT instantiation)
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

  # System health monitoring (safety-critical)
  zephyr_library_sources_ifdef(CONFIG_LQ_SYSTEM_HEALTH
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_system_health.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_system_health.c
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

  # Zephyr device drivers (devicetree instantiation)
  # Hardware input drivers
  zephyr_library_sources_ifdef(CONFIG_LQ_HW_ADC
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_hw_adc.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_HW_SPI
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_hw_spi.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_HW_SENSOR
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_hw_sensor.c
  )

  # Signal processing drivers
  zephyr_library_sources_ifdef(CONFIG_LQ_SCALE
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_scale.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_REMAP
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_remap.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_PID
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_pid.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_MID_MERGE
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_mid_merge.c
  )

  # Monitoring and output drivers
  zephyr_library_sources_ifdef(CONFIG_LQ_FAULT_MONITOR
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_fault_monitor.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_VERIFIED_OUTPUT
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_verified_output.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_CYCLIC_OUTPUT
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_cyclic_output.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_GPIO_PATTERN
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_gpio_pattern.c
  )

  # Protocol drivers
  zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_J1939
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_j1939.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_protocol_j1939.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_CANOPEN
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_canopen.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_protocol_canopen.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_ISOTP
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_isotp.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_protocol_isotp.c
  )
  
  zephyr_library_sources_ifdef(CONFIG_LQ_PROTOCOL_UDS
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/drivers/lq_uds.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/zephyr/drivers/lq_protocol_uds.c
  )

  # Public include directories (INTERFACE makes them available to app)
  zephyr_include_directories(
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
  )

  # Include app's generated headers (for lq_config.h with auto-calculated sizes)
  # Must happen after lq_driver library is defined
  zephyr_library_include_directories(${APPLICATION_BINARY_DIR}/app)
endif()

