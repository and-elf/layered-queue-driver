# Platform Adaptors for Embedded Hardware

The Layered Queue Driver now supports generating platform-specific code for flashing directly to embedded hardware. This eliminates the abstraction layer and creates real interrupt handlers and peripheral configurations for your target MCU.

## Supported Platforms

| Platform | Description | SDK/HAL |
|----------|-------------|---------|
| **stm32** | STMicroelectronics STM32 family | STM32 HAL |
| **samd** | Atmel/Microchip SAMD series | Atmel START (ASF4) |
| **esp32** | Espressif ESP32 | ESP-IDF |
| **nrf52** | Nordic nRF52 series | nRF5 SDK |
| **baremetal** | Generic bare-metal (register access) | None |

## Quick Start

### 1. Define Hardware Mappings in DTS

Add hardware-specific properties to your DTS file:

```dts
rpm_adc: rpm-adc-sensor {
    compatible = "lq,hw-adc-input";
    label = "rpm_adc";
    signal_id = <0>;
    
    /* Platform-specific properties */
    hw_instance = <1>;     /* ADC1 */
    hw_channel = <0>;      /* Channel 0 */
};
```

### 2. Generate Platform-Specific Code

```bash
# Generic code (no platform ISRs)
python3 scripts/dts_gen.py app.dts src/

# STM32 HAL code
python3 scripts/dts_gen.py app.dts src/ --platform=stm32

# ESP32 IDF code
python3 scripts/dts_gen.py app.dts src/ --platform=esp32

# Nordic nRF52 SDK code
python3 scripts/dts_gen.py app.dts src/ --platform=nrf52
```

### 3. Integrate into Your Project

The generator creates:
- `lq_generated.h` - Platform-agnostic declarations
- `lq_generated.c` - Core engine initialization
- `lq_platform_hw.c` - **Platform-specific ISRs and peripheral init**

## Platform-Specific Examples

### STM32 HAL

**Generated ISR:**
```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_hw_push(0, (uint32_t)value);  // Signal ID 0
    }
}
```

**CubeMX Configuration:**
1. Enable ADC1 with DMA
2. Configure channels as specified in DTS
3. Enable interrupts: ADC, DMA, SPI
4. Generate HAL initialization code

**Linker Integration:**
The generated ISRs use HAL callbacks, so they integrate automatically with CubeMX-generated code.

### ESP32 IDF

**Generated ISR:**
```c
void lq_adc_read_rpm_adc(void)
{
    int value = adc1_get_raw(ADC1_CHANNEL_0);
    lq_hw_push(0, (uint32_t)value);
}
```

**Integration:**
```c
// In your main task
void sensor_task(void *pvParameters)
{
    while(1) {
        lq_adc_read_rpm_adc();  // Read sensor
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

### Nordic nRF52

**Generated ISR:**
```c
void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE) {
        uint16_t value = p_event->data.done.p_buffer[0];
        lq_hw_push(0, (uint32_t)value);
        
        // Re-trigger conversion
        nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, 1);
    }
}
```

### Bare-Metal

**Generated ISR:**
```c
void ADC_IRQHandler(void)
{
    if (ADC_SR & 0x02) {  /* EOC - End of Conversion */
        uint16_t value = (uint16_t)ADC_DR;
        lq_hw_push(0, (uint32_t)value);
    }
}
```

**Startup Integration:**
Add to your vector table in `startup.s` or linker script.

## DTS Hardware Properties

### Common Properties

| Property | Type | Description |
|----------|------|-------------|
| `hw_instance` | int | Peripheral instance (ADC1=1, SPI2=2, etc.) |
| `hw_channel` | int | Channel number (ADC channel, SPI CS) |
| `hw_cs_pin` | int | Chip select GPIO pin (SPI only) |

### Platform-Specific Examples

#### STM32
```dts
adc_sensor: sensor {
    compatible = "lq,hw-adc-input";
    hw_instance = <1>;      /* ADC1 */
    hw_channel = <5>;       /* Channel 5 (PA5) */
};
```

#### ESP32
```dts
adc_sensor: sensor {
    compatible = "lq,hw-adc-input";
    hw_channel = <0>;       /* ADC1_CHANNEL_0 (GPIO36) */
};
```

#### nRF52
```dts
adc_sensor: sensor {
    compatible = "lq,hw-adc-input";
    hw_channel = <0>;       /* AIN0 (P0.02) */
};
```

## Build System Integration

### CMake Example (STM32)

```cmake
# Generate platform-specific code at build time
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_platform_hw.c
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/dts_gen.py 
            ${CMAKE_CURRENT_SOURCE_DIR}/app.dts
            ${CMAKE_CURRENT_SOURCE_DIR}/src/
            --platform=stm32
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app.dts
    COMMENT "Generating STM32 platform code from DTS"
)

# Add to your executable
add_executable(firmware
    src/main.c
    src/lq_generated.c
    src/lq_platform_hw.c
    # ... other sources
)
```

### Makefile Example (ESP32)

```makefile
COMPONENT_SRCDIRS := src
COMPONENT_ADD_INCLUDEDIRS := include

# Generate platform code
src/lq_platform_hw.c: app.dts
	python3 ../scripts/dts_gen.py $< src/ --platform=esp32

# Add dependency
$(COMPONENT_LIBRARY): src/lq_platform_hw.c
```

## Flash to Hardware

### STM32 (using STM32CubeIDE)

1. Generate code with `--platform=stm32`
2. Import into STM32CubeIDE project
3. Add `lq_platform_hw.c` to build
4. Call `lq_platform_peripherals_init()` in `main()`
5. Build and flash with ST-LINK

### ESP32 (using ESP-IDF)

```bash
# Generate platform code
python3 scripts/dts_gen.py app.dts main/ --platform=esp32

# Build and flash
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### nRF52 (using Segger Embedded Studio)

1. Generate code with `--platform=nrf52`
2. Add to SES project
3. Build and flash with J-Link

## Advanced: Custom Platform

To add support for a new platform, create a new adaptor class:

```python
# In scripts/platform_adaptors.py

class MyPlatformAdaptor(PlatformAdaptor):
    def __init__(self):
        super().__init__("MyPlatform")
    
    def generate_platform_header(self):
        return """#include "my_hal.h" """
    
    def generate_isr_wrapper(self, node, signal_id):
        # Generate ISR code
        return f"void MY_IRQ(void) {{ ... }}"
    
    def generate_peripheral_init(self, hw_inputs):
        # Generate init code
        return "void init(void) { ... }"
```

Then register it in `get_platform_adaptor()`.

## Debugging Tips

1. **Enable platform init:** Define `LQ_PLATFORM_INIT` in your build to call `lq_platform_peripherals_init()`
2. **Check vector table:** Ensure ISR names match your startup code
3. **Verify pin mappings:** Double-check DTS `hw_channel` matches actual hardware
4. **Enable debug output:** Use platform-specific logging in ISRs

## Real-World Example: Automotive ECU on STM32F407

```dts
/ {
    /* 3x RPM sensors on ADC1 channels 0, 1, 2 */
    rpm_primary: rpm-adc-0 {
        compatible = "lq,hw-adc-input";
        hw_instance = <1>;
        hw_channel = <0>;  /* PA0 */
        signal_id = <0>;
    };
    
    rpm_secondary: rpm-adc-1 {
        compatible = "lq,hw-adc-input";
        hw_instance = <1>;
        hw_channel = <1>;  /* PA1 */
        signal_id = <1>;
    };
    
    /* Triple-redundant voting */
    rpm_voted: rpm-voter {
        compatible = "lq,mid-merge";
        input_signal_ids = <0 1 2>;
        output_signal_id = <10>;
        voting_method = "median";
        tolerance = <50>;
    };
    
    /* Output on CAN (J1939 PGN 0xFEF1) at 100Hz */
    rpm_can_output: can-output {
        compatible = "lq,cyclic-output";
        source_signal_id = <10>;
        output_type = "j1939";
        target_id = <65265>;
        period_us = <10000>;  /* 100Hz */
    };
};
```

**Generated Code:** Ready to flash to STM32F407 discovery board!

## Next Steps

- See `samples/stm32/` for complete STM32 example
- Check `samples/esp32/` for ESP32-IDF integration
- Review `docs/devicetree-guide.md` for DTS syntax details
