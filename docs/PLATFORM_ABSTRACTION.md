# Platform Abstraction Strategy

## Overview

The Layered Queue Driver uses a **hybrid platform abstraction approach** that balances portability with flexibility:

```
Application Code
     ↓
LQ Platform API (lq_platform.h)
     ↓
┌────────────────┬──────────────────┬───────────────┐
│ Zephyr RTOS    │  FreeRTOS        │  Bare Metal   │
│ (any hardware) │  (HAL-specific)  │  (HAL-only)   │
└────────────────┴──────────────────┴───────────────┘
     ↓                  ↓                  ↓
Hardware (STM32, ESP32, nRF52, SAMD, etc.)
```

## Design Principles

### 1. **Use Zephyr API When Available**
- **Rationale**: Zephyr provides a unified, portable API across 400+ boards
- **Benefits**:
  - Single codebase works on STM32, ESP32, nRF52, SAMD, etc.
  - Leverages Zephyr's devicetree for hardware configuration
  - Automatic driver selection and initialization
  - Well-tested, production-ready drivers

### 2. **Fall Back to Platform HAL for Bare Metal**
- **Rationale**: Some projects need minimal overhead or specific hardware features
- **Benefits**:
  - No RTOS overhead
  - Direct hardware access when needed
  - Can use vendor-specific features

### 3. **Support FreeRTOS with HAL**
- **Rationale**: Many existing projects use FreeRTOS
- **Benefits**:
  - Compatible with existing codebases
  - Smaller footprint than Zephyr
  - Familiar to embedded developers

## Platform Selection Matrix

| PLATFORM | RTOS      | Implementation File(s)                  | Use Case                           |
|----------|-----------|----------------------------------------|------------------------------------|
| stm32    | zephyr    | `lq_platform_*_zephyr.c`               | Portable STM32 project             |
| stm32    | freertos  | `lq_platform_*_stm32.c` + FreeRTOS    | Legacy STM32 + FreeRTOS project    |
| stm32    | baremetal | `lq_platform_*_stm32.c`                | Bare-metal STM32 (no RTOS)         |
| esp32    | zephyr    | `lq_platform_*_zephyr.c`               | Portable ESP32 project             |
| esp32    | freertos  | `lq_platform_*_esp32.c` + ESP-IDF     | ESP32 with native ESP-IDF          |
| nrf52    | zephyr    | `lq_platform_*_zephyr.c`               | Nordic with Zephyr                 |
| nrf52    | baremetal | `lq_platform_*_nrf52.c`                | Nordic bare-metal (nRF SDK)        |
| native   | baremetal | `lq_platform_native.c`                 | Unit tests, HIL testing            |

## Implementation Structure

### Zephyr Implementations
Located in `src/platform/lq_platform_*_zephyr.c`:
- `lq_platform_uart_zephyr.c` - UART using Zephyr UART driver
- `lq_platform_gpio_zephyr.c` - GPIO using Zephyr GPIO driver
- `lq_platform_pwm_zephyr.c` - PWM using Zephyr PWM driver
- `lq_platform_spi_zephyr.c` - SPI using Zephyr SPI driver
- `lq_platform_i2c_zephyr.c` - I2C using Zephyr I2C driver

These files:
- Use `#ifdef __ZEPHYR__` guards
- Reference devicetree nodes: `DT_NODELABEL(uart0)`
- Use Zephyr driver APIs: `uart_poll_out()`, `gpio_pin_set()`, etc.
- Automatically included when `RTOS=zephyr`

### Platform-Specific HAL Implementations
Located in `src/platform/lq_platform_<hw>.c`:
- `lq_platform_stm32.c` - STM32 HAL (works with/without FreeRTOS)
- `lq_platform_esp32.c` - ESP-IDF (includes FreeRTOS)
- `lq_platform_nrf52.c` - nRF SDK
- `lq_platform_samd.c` - Microchip/Atmel ASF

These files:
- Use vendor HAL APIs directly
- Platform-specific initialization
- Used when `RTOS=baremetal` or `RTOS=freertos`

## API Design Pattern

### Registration-Based Approach (Zephyr)

For Zephyr, peripherals are registered at init from devicetree:

```c
/* In generated initialization code */
void app_platform_init(void) {
    /* GPIO registration from DTS */
    lq_gpio_register(0, DEVICE_DT_GET(DT_NODELABEL(gpio0)), 5, GPIO_OUTPUT);
    lq_gpio_register(1, DEVICE_DT_GET(DT_NODELABEL(gpio0)), 12, GPIO_INPUT);
    
    /* PWM registration from DTS */
    lq_pwm_register(0, DEVICE_DT_GET(DT_NODELABEL(pwm0)), 1, 0);
    
    /* SPI registration from DTS */
    const struct gpio_dt_spec cs = GPIO_DT_SPEC_GET(DT_NODELABEL(my_sensor), cs_gpios);
    lq_spi_register(0, DEVICE_DT_GET(DT_NODELABEL(spi0)), &cs, 1000000, SPI_OP_MODE_MASTER);
}

/* Application uses simple indexed API */
lq_gpio_set(0, true);           /* GPIO pin 0 (mapped to GPIO0.5) */
lq_pwm_set(0, 7500, 1000);      /* PWM channel 0, 75%, 1kHz */
lq_spi_send(0, data, len);      /* SPI device 0 */
```

### Direct Mapping Approach (HAL)

For bare-metal/FreeRTOS, pins map directly to hardware:

```c
/* STM32 HAL implementation */
int lq_gpio_set(uint8_t pin, bool value) {
    GPIO_TypeDef *port = get_gpio_port(pin);
    uint16_t pin_num = get_gpio_pin(pin);
    HAL_GPIO_WritePin(port, pin_num, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}
```

## CMake Integration

The build system selects platform files based on `PLATFORM` and `RTOS`:

```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM stm32      # Hardware: STM32
    RTOS zephyr         # OS: Zephyr RTOS
    ENABLE_HIL_TESTS
)
```

CMake logic (in `cmake/LayeredQueueApp.cmake`):
```cmake
if(APP_RTOS STREQUAL "zephyr")
    # Use Zephyr peripheral implementations
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_ROOT}/src/platform/lq_platform_zephyr.c
        ${LQ_ROOT}/src/platform/lq_platform_uart_zephyr.c
        ${LQ_ROOT}/src/platform/lq_platform_gpio_zephyr.c
        ${LQ_ROOT}/src/platform/lq_platform_pwm_zephyr.c
        ${LQ_ROOT}/src/platform/lq_platform_spi_zephyr.c
        ${LQ_ROOT}/src/platform/lq_platform_i2c_zephyr.c
    )
    target_compile_definitions(${TARGET_NAME} PRIVATE __ZEPHYR__=1)
else()
    # Use platform-specific HAL
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_ROOT}/src/platform/lq_platform_${APP_PLATFORM}.c
    )
endif()
```

## Example Scenarios

### Scenario 1: Portable Multi-Platform Project
**Goal**: Same application runs on STM32, ESP32, nRF52

**Solution**: Use `RTOS=zephyr`
```cmake
# Works on any Zephyr-supported board
add_lq_application(motor_control
    DTS motor.dts
    PLATFORM stm32    # Or esp32, nrf52, samd, etc.
    RTOS zephyr
)
```

**DTS file** defines hardware abstraction:
```dts
/ {
    motor_pwm: pwm@0 {
        compatible = "pwm-leds";
        pwms = <&pwm0 1 PWM_MSEC(20) PWM_POLARITY_NORMAL>;
    };
    
    encoder_gpio: gpio@0 {
        compatible = "gpio-keys";
        gpios = <&gpio0 12 GPIO_ACTIVE_HIGH>;
    };
};
```

### Scenario 2: Legacy STM32 + FreeRTOS Project
**Goal**: Integrate with existing STM32 HAL + FreeRTOS codebase

**Solution**: Use `PLATFORM=stm32 RTOS=freertos`
```cmake
add_lq_application(legacy_app
    DTS legacy.dts
    PLATFORM stm32
    RTOS freertos
    SOURCES legacy_hal_init.c
)
```

Implementation uses STM32 HAL directly.

### Scenario 3: Bare-Metal ESP32 with ESP-IDF
**Goal**: Use ESP-IDF features without Zephyr

**Solution**: Use `PLATFORM=esp32 RTOS=baremetal` (ESP-IDF includes FreeRTOS internally)
```cmake
add_lq_application(esp_app
    DTS esp_app.dts
    PLATFORM esp32
    RTOS baremetal  # Uses ESP-IDF
)
```

## Recommendations

### ✅ **Prefer Zephyr When:**
- Building new projects
- Need portability across multiple platforms
- Want devicetree-based configuration
- Using supported hardware (STM32, ESP32, nRF52, SAMD, etc.)
- Team is comfortable with Zephyr ecosystem

### ✅ **Use Platform HAL When:**
- Integrating with existing bare-metal code
- Need specific vendor HAL features
- Want minimal RTOS overhead
- Legacy project migration
- Team expertise in specific vendor HAL

### ✅ **Hybrid Approach:**
Use Zephyr for most peripherals, but add platform-specific code for unique features:
```c
#ifdef __ZEPHYR__
    /* Use Zephyr for standard I/O */
    lq_gpio_set(LED_PIN, true);
#else
    /* Use HAL for special hardware */
    configure_special_timer_mode();
#endif
```

## Future Extensions

### Planned Platform Support
- **RT-Thread**: Chinese RTOS gaining popularity
- **Mbed OS**: ARM ecosystem
- **NuttX**: POSIX-compliant RTOS
- **RIOT**: IoT-focused RTOS

### Auto-Detection
CMake could auto-detect available Zephyr installation:
```cmake
find_package(Zephyr QUIET)
if(Zephyr_FOUND AND NOT APP_RTOS)
    set(APP_RTOS "zephyr")
endif()
```

## Summary

**Primary Strategy**: **Use Zephyr API when RTOS=zephyr**, otherwise use platform-specific HAL.

This provides:
- ✅ **Portability** via Zephyr across 400+ boards
- ✅ **Flexibility** to use bare-metal or FreeRTOS
- ✅ **Simplicity** - one API for applications
- ✅ **Performance** - direct HAL access when needed
- ✅ **Ecosystem** - leverage Zephyr drivers and devicetree
