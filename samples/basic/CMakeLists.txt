# Basic Motor Control Example
# Platform-agnostic - works on any supported platform

# For ESP32 with FreeRTOS
add_lq_application(motor_control_esp32
    DTS ../temperature-control-example.dts
    PLATFORM esp32
    RTOS freertos
)

# For STM32 baremetal
add_lq_application(motor_control_stm32
    DTS ../temperature-control-example.dts
    PLATFORM stm32
    RTOS baremetal
)

# For native/testing with HIL
add_lq_application(motor_control_test
    DTS ../temperature-control-example.dts
    PLATFORM native
    RTOS baremetal
    ENABLE_HIL_TESTS
)
