# Platform Adaptor Quick Reference

## Generate Code for Your Platform

```bash
# Choose your platform:
--platform=stm32      # STMicroelectronics STM32 (HAL)
--platform=samd       # Atmel/Microchip SAMD (ASF4)
--platform=esp32      # Espressif ESP32 (IDF)
--platform=nrf52      # Nordic nRF52 (SDK)
--platform=baremetal  # Generic bare-metal
```

## Hardware Pin Mappings by Platform

### STM32
```dts
sensor: my-sensor {
    compatible = "lq,hw-adc-input";
    hw_instance = <1>;  /* ADC1, ADC2, ADC3 */
    hw_channel = <0>;   /* Channel number (0-15) */
};
```
**Pins:** Channel 0 = PA0, Channel 1 = PA1, etc. (see datasheet)

### ESP32
```dts
sensor: my-sensor {
    compatible = "lq,hw-adc-input";
    hw_channel = <0>;   /* ADC1_CHANNEL_0 through ADC1_CHANNEL_7 */
};
```
**Pins:** 
- Channel 0 = GPIO36
- Channel 1 = GPIO37
- Channel 2 = GPIO38
- Channel 3 = GPIO39
- Channel 4 = GPIO32
- Channel 5 = GPIO33
- Channel 6 = GPIO34
- Channel 7 = GPIO35

### nRF52
```dts
sensor: my-sensor {
    compatible = "lq,hw-adc-input";
    hw_channel = <0>;   /* AIN0 through AIN7 */
};
```
**Pins:**
- AIN0 = P0.02
- AIN1 = P0.03
- AIN2 = P0.04
- AIN3 = P0.05
- AIN4 = P0.28
- AIN5 = P0.29
- AIN6 = P0.30
- AIN7 = P0.31

### SAMD21/SAMD51
```dts
sensor: my-sensor {
    compatible = "lq,hw-adc-input";
    hw_channel = <0>;   /* ADC channel 0-19 */
};
```

## Integration Checklist

### ✅ STM32 + CubeMX
1. Configure peripherals in CubeMX (ADC, SPI, DMA)
2. Enable interrupts
3. Generate HAL code
4. Run: `python3 scripts/dts_gen.py app.dts Src/ --platform=stm32`
5. Add `lq_platform_hw.c` to project
6. Call `lq_platform_peripherals_init()` after `MX_Init()`
7. Build and flash

### ✅ ESP32 + ESP-IDF
1. Create `main/app.dts`
2. Run: `python3 ../scripts/dts_gen.py app.dts main/ --platform=esp32`
3. Add to `main/CMakeLists.txt`:
   ```cmake
   idf_component_register(SRCS "lq_generated.c" "lq_platform_hw.c" ...)
   ```
4. Call sensor read functions in task:
   ```c
   void sensor_task(void *pvParameters) {
       while(1) {
           lq_adc_read_rpm_adc();
           vTaskDelay(pdMS_TO_TICKS(10));
       }
   }
   ```
5. Build: `idf.py build`
6. Flash: `idf.py -p /dev/ttyUSB0 flash`

### ✅ nRF52 + SDK
1. Generate code: `python3 scripts/dts_gen.py app.dts src/ --platform=nrf52`
2. Add to Segger Embedded Studio project
3. Call `lq_platform_peripherals_init()` in `main()`
4. Build and flash with J-Link

## Common Issues & Solutions

### "ADC callback not firing" (STM32)
- ✅ Enable DMA in CubeMX
- ✅ Enable ADC interrupt in NVIC
- ✅ Call `HAL_ADC_Start_DMA()`

### "Undefined reference to hadc1" (STM32)
- ✅ CubeMX must generate ADC handle as `extern ADC_HandleTypeDef hadc1;`
- ✅ Or define manually in your code

### "ADC reads always return 0" (ESP32)
- ✅ Check GPIO pin configuration
- ✅ Set attenuation: `adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);`

### "Multiple definition of callback" (NRF52)
- ⚠️ Multiple ADC inputs share one callback
- ✅ Use channel index to distinguish: `p_buffer[channel]`

## File Summary

After generation, you'll have:

| File | Purpose | Platform-Specific? |
|------|---------|-------------------|
| `lq_generated.h` | Declarations | ❌ No |
| `lq_generated.c` | Engine setup | ❌ No |
| `lq_platform_hw.c` | **ISRs & init** | ✅ **YES** |

Only `lq_platform_hw.c` contains platform-specific code. The rest is portable.

## Next: Flash to Real Hardware

1. Connect your development board
2. Verify pin mappings match your DTS
3. Build project
4. Flash and test!

See `docs/platform-adaptors.md` for detailed examples.
