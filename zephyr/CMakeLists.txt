# Zephyr module integration for layered-queue-driver

if(CONFIG_LQ_DRIVER)
  zephyr_library()
  
  zephyr_library_sources(
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/lq_engine.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/lq_hw_input.c
    ${ZEPHYR_CURRENT_MODULE_DIR}/src/platform/lq_platform_zephyr.c
  )
  
  zephyr_library_include_directories(
    ${ZEPHYR_CURRENT_MODULE_DIR}/include
  )
endif()
