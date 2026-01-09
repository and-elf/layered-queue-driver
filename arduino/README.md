# LayeredQueue - Arduino Library

Complete embedded framework for data pipelines and motor control. No device tree or code generation needed!

## Features

- **BLDC Motor Control**: 3-phase motors with complementary PWM and deadtime
  - Multiple modes: 6-step, sinusoidal, FOC, open-loop
  - Supports SAMD21/51 (TCC), ESP32 (MCPWM), STM32 (TIM)
  
- **CAN Protocols**: J1939, CANopen, ISO-TP, UDS
  
- **Signal Processing**: PID control, scaling, remapping, verified outputs
  
- **Diagnostics**: DTC (Diagnostic Trouble Codes) management
  
- **Hardware Abstraction**: Clean API works across platforms

## Supported Boards

| Platform | Boards | PWM Module | Max Motors |
|----------|--------|------------|------------|
| **SAMD21** | Arduino Zero, MKR series, Feather M0 | TCC0 | 1 |
| **SAMD51** | Metro M4, Grand Central M4 | TCC0/TCC1 | 2 |
| **ESP32** | DevKit, NodeMCU-32S | MCPWM | 2 |
| **ESP32-S3** | DevKit-C | MCPWM | 1 |
| **STM32** | Nucleo, BluePill (with STM32 core) | TIM1/TIM8 | Multiple |

## Installation

### Arduino Library Manager (Recommended)

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for **"LayeredQueue BLDC Motor"**
4. Click **Install**

### Manual Installation

**Note**: This library uses symlinks to share code with the main repository. On Windows, you may need to enable Developer Mode or configure Git to handle symlinks.

1. Clone or download this repository
2. Copy the `arduino/` folder to your Arduino libraries folder:
   - Windows: `Documents\Arduino\libraries\LayeredQueue_BLDC\`
   - macOS: `~/Documents/Arduino/libraries/LayeredQueue_BLDC/`
   - Linux: `~/Arduino/libraries/LayeredQueue_BLDC/`
3. Restart Arduino IDE

**Windows users**: If symlinks don't work, see [BUILD_NOTES.md](BUILD_NOTES.md) for solutions.

### PlatformIO

Add to `platformio.ini`:

```ini
lib_deps =
    LayeredQueue BLDC Motor
```

## Quick Start

### BLDC Motor Control (SAMD21)

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  // Configure TCC0 pins
  motor.setHighSidePin(0, 0, 4, 0x04);   // PA04 - Phase U
  motor.setHighSidePin(1, 0, 5, 0x04);   // PA05 - Phase V
  motor.setHighSidePin(2, 0, 6, 0x04);   // PA06 - Phase W
  
  motor.setLowSidePin(0, 0, 10, 0x04);   // PA10 - Phase U'
  motor.setLowSidePin(1, 0, 11, 0x04);   // PA11 - Phase V'
  motor.setLowSidePin(2, 0, 12, 0x04);   // PA12 - Phase W'
  
  motor.begin(3, 7, 25000, 1000);  // 3-phase, 7 pole pairs, 25kHz, 1μs deadtime
  motor.setMode(LQ_BLDC_MODE_SINE);
  motor.enable(true);
}

void loop() {
  motor.update();  // Call regularly
  motor.setPower(50);  // 50% power
  delay(1);
}
```

### ESP32

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  // Configure MCPWM pins
  motor.setHighSidePin(0, 0, 16, 0);  // GPIO16 - Phase U
  motor.setHighSidePin(1, 0, 18, 0);  // GPIO18 - Phase V
  motor.setHighSidePin(2, 0, 19, 0);  // GPIO19 - Phase W
  
  motor.setLowSidePin(0, 0, 17, 0);   // GPIO17 - Phase U'
  motor.setLowSidePin(1, 0, 5, 0);    // GPIO5  - Phase V'
  motor.setLowSidePin(2, 0, 4, 0);    // GPIO4  - Phase W'
  
  motor.begin();
  motor.enable(true);
}

void loop() {
  motor.update();
  motor.setPower(75);
  delay(1);
}
```

## API Reference

### Constructor

```cpp
BLDC_Motor motor(motor_id);
```

- `motor_id`: Motor instance (0-255), for multi-motor systems

### Configuration

```cpp
bool begin(num_phases, pole_pairs, pwm_freq_hz, deadtime_ns);
```

- `num_phases`: Number of motor phases (typically 3)
- `pole_pairs`: Motor pole pairs (check motor datasheet)
- `pwm_freq_hz`: PWM frequency in Hz (20000-50000 typical)
- `deadtime_ns`: Deadtime in nanoseconds (500-2000 typical)
- Returns: `true` on success

```cpp
void setHighSidePin(phase, gpio_port, gpio_pin, alt_func);
void setLowSidePin(phase, gpio_port, gpio_pin, alt_func);
```

- `phase`: Phase number (0-2 for 3-phase)
- `gpio_port`: Port number (0=PORTA, 1=PORTB)
- `gpio_pin`: Pin number within port
- `alt_func`: Peripheral function/mux setting

```cpp
void setMode(mode);
```

- `mode`: 
  - `LQ_BLDC_MODE_6STEP` - Trapezoidal commutation
  - `LQ_BLDC_MODE_SINE` - Sinusoidal PWM (smooth)
  - `LQ_BLDC_MODE_FOC` - Field-oriented control
  - `LQ_BLDC_MODE_OPEN_LOOP` - Open-loop V/f

### Control

```cpp
void enable(bool enable);
```

- Enable or disable motor PWM outputs

```cpp
void setPower(uint8_t power);
```

- Set motor power: 0-100

```cpp
void setDirection(bool forward);
```

- `true` for forward, `false` for reverse

```cpp
void emergencyStop();
```

- Immediately stop motor with active braking

```cpp
void update();
```

- Update motor commutation (call in `loop()` at ~1ms intervals)

### Status

```cpp
uint8_t getPower();
bool isEnabled();
```

## Hardware Connections

### Typical Setup

```
Arduino Board          Motor Driver (DRV8323, etc.)
-------------          ----------------------------
Phase U (high) ------> INHA (High-side U input)
Phase U (low)  ------> INLA (Low-side U input)
Phase V (high) ------> INHB (High-side V input)
Phase V (low)  ------> INLB (Low-side V input)
Phase W (high) ------> INHC (High-side W input)
Phase W (low)  ------> INLC (Low-side W input)
GND           ------> GND (common ground)
```

**IMPORTANT**: 
- Use common ground between Arduino and motor driver
- Motor driver needs separate power supply for motor voltage
- Arduino provides 3.3V logic signals only

## Examples

See `examples/` folder:

- **SAMD_BLDC_Simple** - Basic SAMD21/51 example
- **SAMD51_Dual_Motor** - Two motors on SAMD51
- **ESP32_BLDC_Simple** - Basic ESP32 example

## Pin Reference

### SAMD21 (Arduino Zero, MKR)

| Signal | Pin | TCC Function |
|--------|-----|--------------|
| Phase U | PA04 (D2) | TCC0/WO[0], func E |
| Phase U' | PA10 (D1) | TCC0/WO[4], func E |
| Phase V | PA05 (D3) | TCC0/WO[1], func E |
| Phase V' | PA11 (D0) | TCC0/WO[5], func E |
| Phase W | PA06 (D4) | TCC0/WO[2], func E |
| Phase W' | PA12 (MOSI) | TCC0/WO[6], func E |

### SAMD51 (Metro M4)

See example sketches for detailed pin mappings.

### ESP32

Flexible - any GPIO can be used (see example for recommended pins)

## Safety Guidelines

⚠️ **WARNING**: BLDC motors can be dangerous

1. **Always use deadtime** (500-2000ns) to prevent shoot-through
2. **Test with low voltage** first (5V before 12V/24V)
3. **Secure the motor** - it will spin!
4. **Use current limiting** in hardware
5. **Add emergency stop** button
6. **Never touch** spinning motor

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Motor doesn't spin | Check power supply, pin configuration |
| Motor stutters | Increase PWM frequency, check pole pairs |
| Motor overheats | Reduce max power, add deadtime |
| Compile errors | Check board selection, install dependencies |

## License

Apache-2.0 License

## Library Architecture

This Arduino library is a **thin wrapper** around the main repository's BLDC driver:

- **Core logic**: Shared via symlinks from `../src/drivers/lq_bldc.c`
- **Platform code**: Included from `../src/platform/lq_platform_*.c` based on target board
- **Wrapper**: Arduino-friendly C++ class in this directory

See [BUILD_NOTES.md](BUILD_NOTES.md) for details on how the library is structured and maintained.

## Support

- **Arduino Questions**: See [GETTING_STARTED.md](GETTING_STARTED.md)
- **Build Issues**: See [BUILD_NOTES.md](BUILD_NOTES.md)
- **Main Repository**: https://github.com/yourusername/layered-queue-driver
- **GitHub Issues**: Report bugs and request features
