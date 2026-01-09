# Basic Motor Control Example
# Platform-agnostic - works on any supported platform

# For ESP32
add_lq_application(motor_control_esp32
    DTS ../temperature-control-example.dts
    PLATFORM esp32
)

# For STM32
add_lq_application(motor_control_stm32
    DTS ../temperature-control-example.dts
    PLATFORM stm32
)

# For baremetal/testing
add_lq_application(motor_control_test
    DTS ../temperature-control-example.dts
    PLATFORM baremetal
    ENABLE_HIL_TESTS
)
