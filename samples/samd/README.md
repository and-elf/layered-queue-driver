# SAMD BLDC Motor Example

This example demonstrates BLDC motor control on SAMD21/SAMD51 microcontrollers using the TCC (Timer/Counter for Control) peripheral.

## Hardware Requirements

- **SAMD21-based board**: Arduino Zero, Arduino MKR series, Adafruit Feather M0
- **SAMD51-based board**: Adafruit Metro M4, Adafruit Grand Central M4, Seeeduino XIAO M0
- **BLDC motor**: 3-phase brushless motor (12V or 24V)
- **Motor driver**: 3-phase MOSFET driver (e.g., DRV8323, TMC6200, IR2130)
- **Power supply**: Matched to motor voltage rating

## TCC Peripheral Overview

SAMD TCC modules are specifically designed for motor control:

| Feature | SAMD21 | SAMD51 |
|---------|--------|--------|
| TCC Modules | 3 (TCC0/1/2) | 5 (TCC0/1/2/3/4) |
| Channels per TCC | 4 | 4-6 |
| Waveform Outputs | Up to 8 | Up to 12 |
| Resolution | 24-bit | 24-bit |
| Complementary PWM | ✅ Yes | ✅ Yes |
| Hardware Deadtime | ✅ Yes | ✅ Yes |
| Pattern Generation | ✅ Yes | ✅ Yes |

## Pin Mapping

### SAMD21 (Arduino Zero, MKR)

**TCC0** (recommended for motor control):

| Signal | Pin | Function | TCC Output |
|--------|-----|----------|------------|
| Phase U | PA04 (D2) | TCC0/WO[0] | High-side |
| Phase U_N | PA10 (D1/TX) | TCC0/WO[4] | Low-side |
| Phase V | PA05 (D3) | TCC0/WO[1] | High-side |
| Phase V_N | PA11 (D0/RX) | TCC0/WO[5] | Low-side |
| Phase W | PA06 (D4) | TCC0/WO[2] | High-side |
| Phase W_N | PA12 (MOSI) | TCC0/WO[6] | Low-side |

**Peripheral Mux**: Function **E** (0x04) for TCC0

### SAMD51 (Adafruit Metro M4)

**TCC0** configuration:

| Signal | Pin | Function | TCC Output |
|--------|-----|----------|------------|
| Phase U | PB12 (D10) | TCC0/WO[0] | High-side |
| Phase U_N | PA14 (D2) | TCC0/WO[4] | Low-side |
| Phase V | PB13 (D11) | TCC0/WO[1] | High-side |
| Phase V_N | PA15 (D3) | TCC0/WO[5] | Low-side |
| Phase W | PA20 (D12) | TCC0/WO[2] | High-side |
| Phase W_N | PA16 (D4) | TCC0/WO[6] | Low-side |

**Peripheral Mux**: Function **E** (0x04) for TCC0

## Building

### Arduino IDE

1. Install **Arduino SAMD Boards** or **Adafruit SAMD Boards** package
2. Open `bldc_motor_example.c` (rename to `.ino` if needed)
3. Select your board (e.g., "Arduino Zero", "Adafruit Metro M4")
4. Select the correct serial port
5. Upload

### PlatformIO

```ini
[env:adafruit_metro_m4]
platform = atmelsam
board = adafruit_metro_m4
framework = arduino

[env:arduino_zero]
platform = atmelsam
board = zero
framework = arduino
```

### Bare Metal (Atmel START / ASF4)

1. Generate project with Atmel START
2. Enable **TCC0** peripheral
3. Enable **GCLK** (Generic Clock) for TCC0
4. Add `lq_platform_samd.c` to your project
5. Build with ARM GCC

## Hardware Connections

```
SAMD21/51          Motor Driver (DRV8323)
---------          ----------------------
PA04 (U)    -----> INHA (High-side U)
PA10 (U_N)  -----> INLA (Low-side U)
PA05 (V)    -----> INHB (High-side V)
PA11 (V_N)  -----> INLB (Low-side V)
PA06 (W)    -----> INHC (High-side W)
PA12 (W_N)  -----> INLC (Low-side W)

GND         -----> GND
```

**Power Supply**:
- Motor driver VCC: 12V or 24V (motor voltage)
- SAMD logic: 3.3V from USB or regulator
- **Common ground** between SAMD and motor driver

## Configuration

Edit the configuration in `bldc_motor_example.c`:

```c
struct lq_bldc_config config = {
    .num_phases = 3,
    .pole_pairs = 7,              /* Check motor datasheet */
    .pwm_frequency_hz = 25000,    /* 25 kHz (adjust for EMI/efficiency) */
    .mode = LQ_BLDC_MODE_SINE,    /* Smoother than 6-step */
    .max_duty_cycle = 9500,       /* 95% max */
    .enable_deadtime = true,
    .deadtime_ns = 1000,          /* 1 microsecond (adjust for MOSFETs) */
};
```

### Deadtime Calculation

Deadtime prevents shoot-through in the half-bridge:

```
deadtime_ns >= t_rise + t_fall + safety_margin
```

Typical values:
- **Small MOSFETs** (TO-220): 500-800ns
- **Medium MOSFETs** (D2PAK): 800-1500ns
- **Large MOSFETs** (>100A): 1500-3000ns

## Usage

### Simple Ramp Test

The example includes a built-in ramp test:

```c
void loop(void) {
    static uint8_t throttle = 0;
    static bool increasing = true;
    
    bldc_example_control(throttle);
    
    if (increasing) {
        throttle++;
        if (throttle >= 100) increasing = false;
    } else {
        throttle--;
        if (throttle == 0) increasing = true;
    }
    
    lq_platform_delay_ms(1);  /* 1ms update */
}
```

### Custom Control

```c
void loop(void) {
    /* Read throttle from ADC, joystick, etc. */
    uint8_t throttle = read_throttle_input();
    
    /* Update motor */
    bldc_example_control(throttle);
    
    /* 1ms update rate for smooth control */
    delay(1);
}
```

### Emergency Stop

```c
void emergency_button_isr(void) {
    lq_bldc_emergency_stop(&motor);
    /* Motor is now disabled with active braking */
}
```

## Multi-Motor Support (SAMD51 only)

SAMD51 has multiple TCC modules, allowing control of 2 motors:

```c
struct lq_bldc_motor motor0, motor1;

void setup() {
    /* Motor 0 on TCC0 */
    lq_bldc_init(&motor0, &config0, 0);
    
    /* Motor 1 on TCC1 */
    lq_bldc_init(&motor1, &config1, 1);
    
    lq_bldc_enable(&motor0, true);
    lq_bldc_enable(&motor1, true);
}
```

## Debugging

### Serial Output

```c
void setup() {
    Serial.begin(115200);
    while (!Serial);  /* Wait for serial port */
    
    if (bldc_example_init() != 0) {
        Serial.println("BLDC init failed!");
        while (1);
    }
    
    Serial.println("BLDC motor ready");
}

void loop() {
    bldc_example_control(throttle);
    
    /* Print status every 100ms */
    static uint32_t last_print = 0;
    if (millis() - last_print > 100) {
        Serial.print("Throttle: ");
        Serial.println(throttle);
        last_print = millis();
    }
}
```

### Logic Analyzer

Monitor PWM outputs with a logic analyzer:
- **Frequency**: Should match `pwm_frequency_hz`
- **Deadtime**: Verify gap between complementary edges
- **Duty cycle**: Should ramp smoothly with throttle

## Safety Notes

⚠️ **WARNING**: BLDC motors can spin at high speeds and draw high currents.

1. **Always use proper deadtime** to prevent shoot-through
2. **Test with low voltage first** (e.g., 5V before 24V)
3. **Secure the motor** before enabling (it will spin!)
4. **Use current limiting** in motor driver hardware
5. **Add emergency stop** button/circuit
6. **Avoid touching** motor during operation

## Troubleshooting

| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Motor doesn't spin | No power to driver | Check power supply connections |
| | Wrong pin mapping | Verify TCC pins match hardware |
| | Deadtime too large | Reduce `deadtime_ns` |
| Motor stutters | Low PWM frequency | Increase to 20-40 kHz |
| | Wrong pole pairs | Check motor datasheet |
| Motor overheats | Duty cycle too high | Reduce `max_duty_cycle` |
| | No deadtime | Enable deadtime insertion |
| PWM not visible | GPIO not configured | Check PMUX settings |
| | TCC clock disabled | Enable GCLK and MCLK for TCC |

## References

- [SAMD21 Datasheet](https://www.microchip.com/wwwproducts/en/ATsamd21g18)
- [SAMD51 Datasheet](https://www.microchip.com/wwwproducts/en/ATSAMD51J19A)
- [TCC Peripheral Guide](https://microchipdeveloper.com/32arm:samd21-tc-tcc-overview)
- [DRV8323 Motor Driver Datasheet](https://www.ti.com/product/DRV8323)
- [BLDC Motor Theory](https://www.ti.com/lit/an/sprabq2/sprabq2.pdf)

## License

Copyright (c) 2026 Layered Queue Driver  
SPDX-License-Identifier: Apache-2.0
