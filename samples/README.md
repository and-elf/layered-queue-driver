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
    PLATFORM esp32          # Hardware: stm32, esp32, nrf52, samd, native, etc.
    RTOS freertos           # OS: baremetal, freertos, zephyr, etc.
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

## Platform and RTOS Support

### Hardware Platforms
- **native** - Host PC (for testing/HIL)
- **stm32** - STM32 with HAL and hardware timers
- **esp32** - ESP32 with ESP-IDF and MCPWM
- **nrf52** - Nordic nRF52 with SDK and PPI
- **samd** - Atmel SAMD with TC/TCC

### RTOS/OS Options
- **baremetal** - No RTOS, direct hardware access
- **freertos** - FreeRTOS with tasks and queues
- **zephyr** - Zephyr RTOS integration

The code generator automatically creates platform-specific ISRs, timer setup, peripheral initialization, and RTOS integration code based on the platform and RTOS selection.

## Example DTS Files

- `temperature-control-example.dts` - Temperature sensor with PID control
- `fault-monitor-example.dts` - Fault detection and monitoring
- `joystick-control-example.dts` - Joystick input processing
- `multi-level-fault-example.dts` - Multi-level fault handling
- `multi-output-example.dts` - Multiple output signals

See individual directories for complete application examples with CMakeLists.txt.
