# Platform API Implementation Summary

## Overview

Each hardware platform now has complete implementations for all peripheral types (UART, GPIO, PWM, SPI, I2C).

## Platform Coverage

### ✅ **Zephyr RTOS** (Any Hardware)
**Files**: `src/platform/lq_platform_*_zephyr.c`
- `lq_platform_uart_zephyr.c` - Uses Zephyr UART driver API
- `lq_platform_gpio_zephyr.c` - Uses Zephyr GPIO driver API
- `lq_platform_pwm_zephyr.c` - Uses Zephyr PWM driver API
- `lq_platform_spi_zephyr.c` - Uses Zephyr SPI driver API
- `lq_platform_i2c_zephyr.c` - Uses Zephyr I2C driver API

**Usage**: When `RTOS=zephyr`  
**Benefits**: Portable across 400+ boards, devicetree configuration

---

### ✅ **STM32 (HAL-based)**
**File**: `src/platform/lq_platform_stm32.c`

**Implementations**:
- **UART**: `HAL_UART_Transmit/Receive()` - UART1/2/3 support
- **GPIO**: `HAL_GPIO_WritePin/ReadPin()` - GPIOA-H mapping (pin 0-127)
- **PWM**: TIM2-4 for general PWM (TIM1 reserved for BLDC)
- **SPI**: `HAL_SPI_Transmit/Receive()` - SPI1/2 with CS control
- **I2C**: `HAL_I2C_Master_*/Mem_*()` - I2C1/2 with register access

**Pin Mapping**:
- GPIO: `pin = port_num * 16 + pin_num` (e.g., PA5 = 5, PB12 = 28)
- PWM: Channels 0-3 = TIM2, 4-7 = TIM3, 8-11 = TIM4
- UART: Port 0 = UART1, 1 = UART2, 2 = UART3

**Usage**: `PLATFORM=stm32 RTOS=baremetal` or `RTOS=freertos`

---

### ✅ **ESP32 (ESP-IDF)**
**File**: `src/platform/lq_platform_esp32.c`

**Implementations**:
- **UART**: `uart_write_bytes/read_bytes()` - All UART ports
- **GPIO**: `gpio_set_level/get_level()` - Direct GPIO number
- **PWM**: LEDC peripheral (13-bit resolution, 8 channels)
- **SPI**: `spi_device_transmit()` - SPI master with device handles
- **I2C**: `i2c_master_*()` - I2C master commands

**Features**:
- PWM uses LEDC (Motor Control PWM reserved for BLDC)
- SPI device handles support multiple CS lines
- I2C uses command link pattern for efficient transactions

**Usage**: `PLATFORM=esp32 RTOS=baremetal` (ESP-IDF includes FreeRTOS)

---

### ✅ **AVR (ATmega328P/2560)**
**File**: `src/platform/lq_platform_avr.c`

**Implementations**:
- **UART**: Direct register access (UDR0/1, UCSR0A/1A)
- **GPIO**: PORT/PIN register access (PORTA-F)
- **PWM**: Timer0/1/2 compare outputs (6 channels)
- **SPI**: Hardware SPI via SPDR register
- **I2C**: TWI (Two Wire Interface) master mode

**Pin Mapping**:
- GPIO: `pin = port_num * 8 + bit` (e.g., PB5 = 13, PC2 = 18)
- PWM: Timer channels map to OCR0A/B, OCR1A/B, OCR2A/B
- UART: Port 0 = UART0 (ATmega328), Port 1 = UART1 (ATmega2560+)

**Usage**: `PLATFORM=avr RTOS=baremetal`  
**Note**: AVR is typically bare-metal (no RTOS support yet)

---

### 🔜 **Other Platforms**

Additional platforms have partial implementations:
- **SAMD**: `lq_platform_samd.c` (Microchip/Atmel)
- **nRF52**: Would benefit from platform-specific file
- **FreeRTOS**: `lq_platform_freertos.c` (OS primitives only)

---

## API Consistency

All platforms implement the same API from `lq_platform.h`:

```c
/* UART */
int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length);
int lq_uart_recv(uint8_t port, uint8_t *data, uint16_t length, uint32_t timeout_ms);

/* GPIO */
int lq_gpio_set(uint8_t pin, bool value);
int lq_gpio_get(uint8_t pin, bool *value);
int lq_gpio_toggle(uint8_t pin);

/* PWM */
int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz);
  /* duty_cycle: 0-10000 = 0-100.00% */

/* SPI */
int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length);
int lq_spi_recv(uint8_t cs_pin, uint8_t *data, uint16_t length);
int lq_spi_transceive(uint8_t cs_pin, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length);

/* I2C */
int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length);
int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length);
int lq_i2c_write_read(uint8_t address, 
                      const uint8_t *write_data, uint16_t write_length,
                      uint8_t *read_data, uint16_t read_length);
int lq_i2c_reg_write_byte(uint8_t address, uint8_t reg, uint8_t value);
int lq_i2c_reg_read_byte(uint8_t address, uint8_t reg, uint8_t *value);
int lq_i2c_burst_write(uint8_t address, uint8_t start_reg, const uint8_t *data, uint16_t length);
int lq_i2c_burst_read(uint8_t address, uint8_t start_reg, uint8_t *data, uint16_t length);
int lq_i2c_set_default_bus(uint8_t bus_id);
```

---

## Build System Integration

CMake automatically selects the correct implementation:

```cmake
if(APP_RTOS STREQUAL "zephyr")
    # Use Zephyr peripheral drivers
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_SRC}/platform/lq_platform_zephyr.c
        ${LQ_SRC}/platform/lq_platform_uart_zephyr.c
        ${LQ_SRC}/platform/lq_platform_gpio_zephyr.c
        ${LQ_SRC}/platform/lq_platform_pwm_zephyr.c
        ${LQ_SRC}/platform/lq_platform_spi_zephyr.c
        ${LQ_SRC}/platform/lq_platform_i2c_zephyr.c
    )
elseif(APP_PLATFORM STREQUAL "stm32")
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_SRC}/platform/lq_platform_stm32.c
    )
elseif(APP_PLATFORM STREQUAL "esp32")
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_SRC}/platform/lq_platform_esp32.c
    )
elseif(APP_PLATFORM STREQUAL "avr")
    target_sources(${TARGET_NAME} PRIVATE
        ${LQ_SRC}/platform/lq_platform_avr.c
    )
endif()
```

---

## Usage Examples

### STM32 with Zephyr
```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM stm32
    RTOS zephyr
)
```
Uses: `lq_platform_*_zephyr.c` files

### STM32 Bare-Metal
```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM stm32
    RTOS baremetal
)
```
Uses: `lq_platform_stm32.c`

### ESP32 with ESP-IDF
```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM esp32
    RTOS baremetal
)
```
Uses: `lq_platform_esp32.c`

### AVR (Arduino)
```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM avr
    RTOS baremetal
)
```
Uses: `lq_platform_avr.c`

---

## Platform-Specific Notes

### STM32
- Requires STM32 HAL library
- External handles expected: `huart1-3`, `hspi1-2`, `hi2c1-2`, `htim2-4`
- TIM1 reserved for BLDC motor control
- GPIO pin formula: `port * 16 + pin` (max 128 pins)

### ESP32
- Requires ESP-IDF framework
- MCPWM reserved for BLDC, LEDC used for general PWM
- SPI device handles must be initialized by user code
- I2C uses ESP-IDF command link pattern

### AVR
- Direct register access (no HAL layer)
- Limited to available hardware timers (typically 6 PWM channels)
- Single TWI peripheral (I2C)
- Hardware SPI only (no software SPI yet)

### Zephyr
- Requires devicetree node definitions
- Peripheral registration at runtime via `lq_*_register()` functions
- Works on any Zephyr-supported board
- Most portable option

---

## HIL Testing Support

All platform implementations can be intercepted for HIL testing when using the HIL platform layer:

```c
#ifdef LQ_HIL_ENABLED
    lq_uart_send() → lq_hil_sut_send_uart()
    lq_gpio_set() → lq_hil_sut_send_gpio()
    /* etc. */
#endif
```

See `src/platform/lq_platform_hil.c` for HIL intercept layer.

---

## Testing Status

| Platform | UART | GPIO | PWM | SPI | I2C | Status        |
|----------|------|------|-----|-----|-----|---------------|
| Zephyr   | ✅   | ✅   | ✅  | ✅  | ✅  | Implemented   |
| STM32    | ✅   | ✅   | ✅  | ✅  | ✅  | Implemented   |
| ESP32    | ✅   | ✅   | ✅  | ✅  | ✅  | Implemented   |
| AVR      | ✅   | ✅   | ✅  | ✅  | ✅  | Implemented   |
| SAMD     | ⚠️   | ⚠️   | ⚠️  | ⚠️  | ⚠️  | Partial       |
| nRF52    | 🔜   | 🔜   | 🔜  | 🔜  | 🔜  | Needs work    |
| Native   | ✅   | ✅   | N/A | N/A | N/A | HIL only      |

---

## Migration Guide

### From Arduino
```c
// Arduino
digitalWrite(13, HIGH);
analogWrite(9, 128);

// Layered Queue
lq_gpio_set(13, true);
lq_pwm_set(0, 5000, 1000);  // 50% @ 1kHz
```

### From STM32 HAL
```c
// STM32 HAL
HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
HAL_UART_Transmit(&huart1, data, len, HAL_MAX_DELAY);

// Layered Queue
lq_gpio_set(21, true);  // PB5 = 1*16+5 = 21
lq_uart_send(0, data, len);
```

### From ESP-IDF
```c
// ESP-IDF
gpio_set_level(GPIO_NUM_2, 1);
uart_write_bytes(UART_NUM_0, data, len);

// Layered Queue
lq_gpio_set(2, true);
lq_uart_send(0, data, len);
```

---

## Summary

✅ **All platforms now have complete peripheral API implementations**

**Key Points**:
1. **Zephyr**: Use when portability is priority
2. **STM32/ESP32/AVR**: Use when platform-specific features needed
3. **HIL**: All platforms support Hardware-in-Loop testing
4. **Consistent API**: Same code works across all platforms
5. **Build System**: Automatic platform selection via CMake

**Recommendation**: Start with **Zephyr** for new projects, fall back to platform HAL only when needed.
