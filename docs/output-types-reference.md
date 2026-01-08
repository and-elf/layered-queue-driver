# Output Types Reference

The layered queue driver supports multiple output types for dispatching processed sensor data to various hardware interfaces and protocols.

## Supported Output Types

### 1. GPIO Output (`LQ_OUTPUT_GPIO`)
**Use Case:** Digital outputs (LEDs, relays, solenoids)

**Device Tree:**
```dts
status_led: lq-cyclic-output@0 {
    compatible = "lq,cyclic-output";
    source-signal-id = <0>;
    output-type = "gpio";
    target-id = <5>;              /* GPIO pin number */
    period-us = <100000>;
};
```

**Platform Function:**
```c
int lq_gpio_set(uint8_t pin, bool state);
```

---

### 2. PWM Output (`LQ_OUTPUT_PWM`)
**Use Case:** Motor control, LED dimming, servo control

**Device Tree:**
```dts
fan_pwm: lq-cyclic-output@1 {
    compatible = "lq,cyclic-output";
    source-signal-id = <0>;
    output-type = "pwm";
    target-id = <2>;              /* PWM channel */
    period-us = <50000>;
};
```

**Platform Function:**
```c
int lq_pwm_set(uint8_t channel, uint32_t duty_cycle);
```

**Value Encoding:** `value` field contains duty cycle (0-100% or raw counts)

---

### 3. DAC Output (`LQ_OUTPUT_DAC`)
**Use Case:** Analog output, analog gauges, voltage control

**Device Tree:**
```dts
analog_gauge: lq-cyclic-output@2 {
    compatible = "lq,cyclic-output";
    source-signal-id = <1>;
    output-type = "dac";
    target-id = <0>;              /* DAC channel */
    period-us = <20000>;
};
```

**Platform Function:**
```c
int lq_dac_write(uint8_t channel, uint16_t value);
```

**Value Encoding:** `value` field contains DAC counts (typically 0-4095 for 12-bit DAC)

---

### 4. SPI Output (`LQ_OUTPUT_SPI`)
**Use Case:** SPI peripherals, displays, external ADCs/DACs

**Device Tree:**
```dts
spi_display: lq-cyclic-output@3 {
    compatible = "lq,cyclic-output";
    source-signal-id = <1>;
    output-type = "spi";
    target-id = <1>;              /* SPI device/CS number */
    period-us = <100000>;
};
```

**Platform Function:**
```c
int lq_spi_send(uint8_t device, const uint8_t *data, size_t len);
```

**Value Encoding:** `value` (32-bit) is sent as 4 bytes: `[LSB, byte1, byte2, MSB]`

---

### 5. I2C Output (`LQ_OUTPUT_I2C`)
**Use Case:** I2C peripherals, sensors, EEPROM, displays

**Device Tree:**
```dts
i2c_output: lq-cyclic-output@4 {
    compatible = "lq,cyclic-output";
    source-signal-id = <2>;
    output-type = "i2c";
    target-id = <0x5010>;         /* bits[15:8]=addr, bits[7:0]=register */
    period-us = <200000>;
};
```

**Platform Function:**
```c
int lq_i2c_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);
```

**Target ID Encoding:**
- Bits [15:8]: I2C slave address
- Bits [7:0]: Register address

**Value Encoding:** `value` (32-bit) is sent as 4 bytes to the specified register

---

### 6. UART Output (`LQ_OUTPUT_UART`)
**Use Case:** Serial diagnostics, logging, RS-232/RS-485 communication

**Device Tree:**
```dts
uart_diag: lq-cyclic-output@5 {
    compatible = "lq,cyclic-output";
    source-signal-id = <0>;
    output-type = "uart";
    target-id = <0>;              /* UART port number */
    period-us = <1000000>;
};
```

**Platform Function:**
```c
int lq_uart_send(const uint8_t *data, size_t len);
```

**Value Encoding:** `value` is converted to ASCII string with newline: `"12345\n"`

---

### 7. CAN Output (`LQ_OUTPUT_CAN`)
**Use Case:** Raw CAN bus communication

**Device Tree:**
```dts
can_output: lq-cyclic-output@6 {
    compatible = "lq,cyclic-output";
    source-signal-id = <1>;
    output-type = "can";
    target-id = <0x123>;          /* CAN message ID */
    period-us = <100000>;
};
```

**Platform Function:**
```c
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);
```

**Value Encoding:** `value` (32-bit) is sent as 4 bytes in CAN data field
**Flags:** `flags & 1`: 0=standard ID, 1=extended ID

---

### 8. J1939 Output (`LQ_OUTPUT_J1939`)
**Use Case:** Automotive/heavy equipment CAN communication (SAE J1939)

**Device Tree:**
```dts
j1939_rpm: lq-cyclic-output@7 {
    compatible = "lq,cyclic-output";
    source-signal-id = <1>;
    output-type = "j1939";
    target-id = <0xF004>;         /* PGN (Parameter Group Number) */
    period-us = <100000>;
};
```

**Platform Function:**
```c
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);
```

**Target ID:** PGN (Parameter Group Number)
**Value Encoding:** `value` (32-bit) is sent as 4 bytes, CAN ID built from PGN

---

### 9. CANopen Output (`LQ_OUTPUT_CANOPEN`)
**Use Case:** Industrial automation CAN communication (CANopen)

**Device Tree:**
```dts
canopen_tpdo: lq-cyclic-output@8 {
    compatible = "lq,cyclic-output";
    source-signal-id = <0>;
    output-type = "canopen";
    target-id = <0x180>;          /* COB-ID (TPDO base) */
    period-us = <200000>;
};
```

**Platform Function:**
```c
int lq_can_send(uint32_t can_id, bool is_extended, const uint8_t *data, uint8_t len);
```

**Target ID:** COB-ID (Communication Object Identifier)
**Value Encoding:** `value` (32-bit) is sent as 4 bytes in PDO

---

### 10. Modbus Output (`LQ_OUTPUT_MODBUS`)
**Use Case:** Industrial PLC/SCADA communication (Modbus RTU/TCP)

**Device Tree:**
```dts
modbus_output: lq-cyclic-output@9 {
    compatible = "lq,cyclic-output";
    source-signal-id = <2>;
    output-type = "modbus";
    target-id = <0x010100>;       /* bits[23:16]=slave, bits[15:0]=register */
    period-us = <500000>;
};
```

**Platform Function:**
```c
int lq_modbus_write(uint8_t slave_id, uint16_t reg, uint16_t value);
```

**Target ID Encoding:**
- Bits [23:16]: Modbus slave ID
- Bits [15:0]: Register address

**Value Encoding:** `value` (lower 16 bits) written to holding register

---

## Platform Implementation

All platform functions have weak stub implementations that can be overridden:

```c
/* Example platform implementation for STM32 */
int lq_pwm_set(uint8_t channel, uint32_t duty_cycle) {
    if (channel == 0) {
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, duty_cycle);
        return 0;
    }
    return -1;
}

int lq_gpio_set(uint8_t pin, bool state) {
    HAL_GPIO_WritePin(GPIOA, (1 << pin), state ? GPIO_PIN_SET : GPIO_PIN_RESET);
    return 0;
}
```

## Usage Pattern

After calling `lq_engine_step()`, dispatch outputs to hardware:

```c
void application_main_loop(void) {
    while (1) {
        lq_engine_step(&g_lq_engine, timestamp_us);
        lq_generated_dispatch_outputs();  /* Auto-generated */
        delay_ms(10);
    }
}
```

The dispatch function is automatically generated based on device tree configuration and only includes code for output types actually used.

## Complete Example

See [samples/multi-output-example.dts](../samples/multi-output-example.dts) for a comprehensive example using all output types.
