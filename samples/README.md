# Layered Queue Driver - Sample Applications

This directory contains sample device tree configurations for various use cases.

## Directory Structure

- **automotive/** - Multi-sensor fusion with voting and fault tolerance
- **canopen/** - CANopen motor controller with EDS expansion
- **j1939/** - SAE J1939 automotive CAN bus communication
- **basic/** - Simple examples for getting started
- **\*.dts** - Standalone example device trees

## Using Samples

All samples are platform-agnostic. Simply specify your target platform when building:

```cmake
add_lq_application(my_motor_controller
    DTS motor_system.dts
    PLATFORM esp32          # or: stm32, nrf52, samd, baremetal, etc.
)
```

### With CANopen EDS Expansion

```cmake
add_lq_application(canopen_motor
    DTS motor_system.dts
    EDS example_motor.eds
    PLATFORM stm32
)
```

### With HIL Testing

```cmake
add_lq_application(automotive_app
    DTS app.dts
    PLATFORM baremetal
    ENABLE_HIL_TESTS
)
```

Then run:
```bash
cmake --build build --target automotive_app_hil_run
```

## Platform Support

Supported platforms:
- **baremetal** - Generic C implementation (no RTOS)
- **stm32** - STM32 HAL with hardware timers
- **esp32** - ESP-IDF with MCPWM
- **nrf52** - Nordic SDK with PPI
- **samd** - Atmel SAMD with TC/TCC
- **freertos** - FreeRTOS wrapper
- **zephyr** - Zephyr RTOS integration

The code generator automatically creates platform-specific ISRs, timer setup, and peripheral initialization based on the platform selection.

## Example DTS Files

- `temperature-control-example.dts` - Temperature sensor with PID control
- `fault-monitor-example.dts` - Fault detection and monitoring
- `joystick-control-example.dts` - Joystick input processing
- `multi-level-fault-example.dts` - Multi-level fault handling
- `multi-output-example.dts` - Multiple output signals

See individual directories for complete application examples with CMakeLists.txt.
