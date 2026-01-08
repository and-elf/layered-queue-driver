# Platform Portability Guide

## Overview

The layered queue driver uses platform abstraction to support both GNU and non-GNU toolchains. Platform-specific hardware functions use `extern` declarations in generated code with two implementation options:

1. **GNU toolchains (GCC, Clang)**: Weak symbols allow optional override
2. **Non-GNU toolchains (MSVC, IAR, ARMCC)**: Standard extern with required implementations

## Generated Code Approach

The code generator (`dts_gen.py`) creates **portable `extern` declarations** based on device tree configuration:

```c
/* Generated in lq_generated.c */
extern int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);
extern int lq_gpio_set(uint8_t pin, bool state);
extern int lq_pwm_set(uint8_t channel, uint32_t duty_cycle);
// ... only functions actually used by your DTS
```

## Implementation Options

### Option 1: Link Platform Stubs (Recommended for Testing)

Link `src/platform/lq_platform_stubs.c` which provides:
- **GNU**: Weak symbols (can be overridden)
- **Non-GNU**: Regular implementations (must not duplicate)

```cmake
# CMakeLists.txt
add_executable(my_app
    src/main.c
    src/lq_generated.c
)
target_link_libraries(my_app layered_queue)  # Includes stubs
```

The stubs file automatically detects GNU vs non-GNU:

```c
/* lq_platform_stubs.c */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    return 0;  /* No-op stub */
}
```

### Option 2: Provide Your Own Implementations

Don't link stubs, implement all required functions yourself:

```c
/* my_platform.c */
#include "lq_platform.h"

int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    // Your CAN driver implementation
    HAL_CAN_Transmit(can_id, is_extended, data, len);
    return 0;
}

int lq_pwm_set(uint8_t channel, uint32_t duty_cycle) {
    // Your PWM implementation
    __HAL_TIM_SET_COMPARE(&htim1, channel, duty_cycle);
    return 0;
}
```

### Option 3: Override Weak Symbols (GNU Only)

Link stubs **and** provide your implementations - weak symbols are automatically overridden:

```c
/* my_can_driver.c - overrides weak stub */
#include "lq_platform.h"

int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    return my_hardware_can_send(can_id, is_extended, data, len);
}
```

## Toolchain Compatibility

### GNU Toolchains (GCC, Clang)
✅ Weak symbols supported
✅ Can mix stubs + overrides
✅ Linker automatically selects strong symbols

**Usage:**
```bash
gcc -o app main.c lq_generated.c -llayered_queue
# Stubs are linked, can be overridden
```

### IAR Embedded Workbench
❌ Weak symbols not supported
✅ Use extern with custom implementations
✅ Or use linker multi-definition options

**Usage:**
```
// Option A: Don't link stubs
iccarm main.c lq_generated.c my_platform.c -llayered_queue_core

// Option B: Use multi-definition flag (if supported)
iccarm --allow-multiple-definition ...
```

### ARM Compiler (ARMCC)
⚠️ Limited weak symbol support
✅ Use extern with custom implementations

**Usage:**
```bash
armcc --c99 main.c lq_generated.c my_platform.c
```

### MSVC (Microsoft Visual C++)
❌ No weak symbols
✅ Use __declspec(selectany) as alternative
✅ Or extern with required implementations

**Usage:**
```cmd
cl /c main.c lq_generated.c my_platform.c
link main.obj lq_generated.obj my_platform.obj layered_queue.lib
```

## Platform Function Reference

All platform functions declared in [`include/lq_platform.h`](../include/lq_platform.h):

| Function | Purpose | Parameters |
|----------|---------|------------|
| `lq_can_send()` | Transmit CAN message | ID, extended flag, data, length |
| `lq_gpio_set()` | Set GPIO output | Pin number, state |
| `lq_uart_send()` | Transmit UART data | Data buffer, length |
| `lq_spi_send()` | Transmit SPI data | Device/CS, data, length |
| `lq_i2c_write()` | Write I2C register | Address, register, data, length |
| `lq_pwm_set()` | Set PWM duty cycle | Channel, duty cycle |
| `lq_dac_write()` | Write DAC value | Channel, value |
| `lq_modbus_write()` | Write Modbus register | Slave ID, register, value |

## Build System Integration

### CMake Example

```cmake
# For production with real hardware
add_executable(production_app
    src/main.c
    src/my_stm32_platform.c  # Your implementations
    ${GENERATED_SOURCES}
)
target_link_libraries(production_app layered_queue)

# For testing without hardware
add_executable(test_app
    tests/test_main.cpp
    ${GENERATED_SOURCES}
)
target_link_libraries(test_app 
    layered_queue  # Includes stubs
    gtest
)
```

### Makefile Example

```makefile
# Production build
production: main.o my_platform.o lq_generated.o
	$(CC) $^ -llayered_queue -o $@

# Test build (includes stubs)
test: test_main.o lq_generated.o
	$(CC) $^ -llayered_queue -o $@
```

## Best Practices

1. **For testing**: Always link `lq_platform_stubs.c` to build without hardware
2. **For production**: Implement only the functions your device tree uses
3. **Cross-platform**: Use `extern` declarations, avoid GNU-specific features in your code
4. **Verification**: Check generated `lq_generated.c` to see which functions are required

## Example: STM32 Platform Implementation

```c
/* stm32_platform.c */
#include "lq_platform.h"
#include "stm32f4xx_hal.h"

extern CAN_HandleTypeDef hcan1;
extern TIM_HandleTypeDef htim1;

int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len) {
    CAN_TxHeaderTypeDef header = {
        .StdId = is_extended ? 0 : can_id,
        .ExtId = is_extended ? can_id : 0,
        .IDE = is_extended ? CAN_ID_EXT : CAN_ID_STD,
        .RTR = CAN_RTR_DATA,
        .DLC = len,
    };
    
    uint32_t mailbox;
    return HAL_CAN_AddTxMessage(&hcan1, &header, (uint8_t*)data, &mailbox) == HAL_OK ? 0 : -1;
}

int lq_pwm_set(uint8_t channel, uint32_t duty_cycle) {
    __HAL_TIM_SET_COMPARE(&htim1, channel << 2, duty_cycle);
    return 0;
}

int lq_gpio_set(uint8_t pin, bool state) {
    HAL_GPIO_WritePin(GPIOA, 1 << pin, state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}
```

## Troubleshooting

### Linker Error: Undefined Reference

**Problem:** `undefined reference to 'lq_can_send'`

**Solution:**
- Link `lq_platform_stubs.c`, OR
- Implement the function in your platform code

### Multiple Definition Error (Non-GNU)

**Problem:** `multiple definition of 'lq_can_send'`

**Solution:**
- Don't link `lq_platform_stubs.c` if providing your own implementations, OR
- Check your linker supports `--allow-multiple-definition`

### Weak Symbols Not Working

**Problem:** Your implementation not used, stub still runs

**Solution:**
- Verify compiler supports weak symbols (`__GNUC__` defined)
- Check link order: your `.o` files should come before library
- Use `nm` tool to verify symbols: `nm -C app.o | grep lq_can_send`

## Migration from Weak-Only Approach

If migrating from an older version with `__attribute__((weak))` in generated code:

1. ✅ New generated code uses `extern` (portable)
2. ✅ Stubs file uses conditional `__attribute__((weak))`
3. ✅ No changes needed to your platform code
4. ✅ Both GNU and non-GNU toolchains now supported
