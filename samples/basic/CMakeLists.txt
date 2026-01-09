# Basic Example
# Platform-agnostic - works on any supported platform

# For ESP32 with FreeRTOS
add_lq_application(basic_esp32
    DTS ../automotive/app.dts
    PLATFORM esp32
    RTOS freertos
)

# For STM32 baremetal
add_lq_application(basic_stm32
    DTS ../automotive/app.dts
    PLATFORM stm32
    RTOS baremetal
)

# For native/testing with HIL
add_lq_application(basic_native
    DTS ../automotive/app.dts
    PLATFORM native
    RTOS baremetal
    ENABLE_HIL_TESTS
)
