# Arduino Library - Getting Started Guide

This guide shows how to use the LayeredQueue BLDC Motor library in Arduino without needing device tree code generation.

## Key Differences from Main Repository

| Feature | Main Repo | Arduino Library |
|---------|-----------|-----------------|
| **Configuration** | Device tree (DTS) files | Direct C++ API calls |
| **Code Generation** | Python scripts generate code | No generation needed |
| **Platform Layer** | Multiple backend options | Arduino-specific backends |
| **Dependencies** | Zephyr/FreeRTOS optional | Arduino core only |
| **Initialization** | DTS-driven | Manual `begin()` calls |

## Installation

### Method 1: Arduino Library Manager (Easiest)

1. Open Arduino IDE
2. **Tools → Manage Libraries**
3. Search: `LayeredQueue BLDC`
4. Click **Install**

### Method 2: GitHub (Latest Development)

```bash
cd ~/Arduino/libraries/
git clone https://github.com/yourrepo/layered-queue-driver LayeredQueue_BLDC
cd LayeredQueue_BLDC/arduino
```

Or download ZIP and install via **Sketch → Include Library → Add .ZIP Library**

## Quick Start Examples

### Example 1: Simple Motor Control (SAMD21)

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  Serial.begin(115200);
  
  // Configure pins for Arduino Zero
  motor.setHighSidePin(0, 0, 4, 0x04);   // PA04
  motor.setHighSidePin(1, 0, 5, 0x04);   // PA05
  motor.setHighSidePin(2, 0, 6, 0x04);   // PA06
  
  motor.setLowSidePin(0, 0, 10, 0x04);   // PA10
  motor.setLowSidePin(1, 0, 11, 0x04);   // PA11
  motor.setLowSidePin(2, 0, 12, 0x04);   // PA12
  
  // Initialize: 3 phases, 7 pole pairs, 25kHz PWM, 1μs deadtime
  if (!motor.begin(3, 7, 25000, 1000)) {
    Serial.println("Motor init failed!");
    while(1);
  }
  
  motor.setMode(LQ_BLDC_MODE_SINE);
  motor.enable(true);
}

void loop() {
  motor.update();        // Update commutation
  motor.setPower(50);    // 50% power
  delay(1);              // 1ms loop
}
```

### Example 2: Variable Speed (ESP32)

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);
int potPin = 34;  // ADC pin for potentiometer

void setup() {
  // Configure MCPWM pins
  motor.setHighSidePin(0, 0, 16, 0);  // GPIO16
  motor.setHighSidePin(1, 0, 18, 0);  // GPIO18
  motor.setHighSidePin(2, 0, 19, 0);  // GPIO19
  
  motor.setLowSidePin(0, 0, 17, 0);   // GPIO17
  motor.setLowSidePin(1, 0, 5, 0);    // GPIO5
  motor.setLowSidePin(2, 0, 4, 0);    // GPIO4
  
  motor.begin();
  motor.enable(true);
}

void loop() {
  motor.update();
  
  // Read potentiometer (0-4095)
  int raw = analogRead(potPin);
  uint8_t speed = map(raw, 0, 4095, 0, 100);
  
  motor.setPower(speed);
  delay(1);
}
```

### Example 3: Direction Control

```cpp
void loop() {
  static bool forward = true;
  static uint32_t lastSwitch = 0;
  
  motor.update();
  motor.setPower(75);
  
  // Reverse direction every 5 seconds
  if (millis() - lastSwitch > 5000) {
    forward = !forward;
    motor.setDirection(forward);
    lastSwitch = millis();
  }
  
  delay(1);
}
```

### Example 4: Emergency Stop Button

```cpp
const int ESTOP_PIN = 2;

void setup() {
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ESTOP_PIN), emergencyStop, FALLING);
  
  motor.begin();
  motor.enable(true);
}

void emergencyStop() {
  motor.emergencyStop();  // Immediate brake
}

void loop() {
  if (digitalRead(ESTOP_PIN) == HIGH) {
    motor.update();
    motor.setPower(60);
  }
  delay(1);
}
```

## Pin Configuration by Board

### Arduino Zero / MKR Series

```cpp
// TCC0 pins (Function E = 0x04)
motor.setHighSidePin(0, 0, 4, 0x04);   // PA04 (D2)
motor.setHighSidePin(1, 0, 5, 0x04);   // PA05 (D3)
motor.setHighSidePin(2, 0, 6, 0x04);   // PA06 (D4)

motor.setLowSidePin(0, 0, 10, 0x04);   // PA10 (D1/TX)
motor.setLowSidePin(1, 0, 11, 0x04);   // PA11 (D0/RX)
motor.setLowSidePin(2, 0, 12, 0x04);   // PA12 (MOSI)
```

### Adafruit Metro M4 / Grand Central

```cpp
// TCC0 pins
motor.setHighSidePin(0, 1, 12, 0x04);  // PB12 (D10)
motor.setHighSidePin(1, 1, 13, 0x04);  // PB13 (D11)
motor.setHighSidePin(2, 0, 20, 0x04);  // PA20 (D12)

motor.setLowSidePin(0, 0, 14, 0x04);   // PA14 (D2)
motor.setLowSidePin(1, 0, 15, 0x04);   // PA15 (D3)
motor.setLowSidePin(2, 0, 16, 0x04);   // PA16 (D4)
```

### ESP32 DevKit

```cpp
// MCPWM Unit 0 (gpio_port unused on ESP32)
motor.setHighSidePin(0, 0, 16, 0);     // GPIO16
motor.setHighSidePin(1, 0, 18, 0);     // GPIO18
motor.setHighSidePin(2, 0, 19, 0);     // GPIO19

motor.setLowSidePin(0, 0, 17, 0);      // GPIO17
motor.setLowSidePin(1, 0, 5, 0);       // GPIO5
motor.setLowSidePin(2, 0, 4, 0);       // GPIO4
```

## API Methods

### Configuration

```cpp
// Initialize motor
bool begin(num_phases=3, pole_pairs=7, pwm_freq_hz=25000, deadtime_ns=1000);

// Configure pins (call before begin())
void setHighSidePin(phase, gpio_port, gpio_pin, alt_func);
void setLowSidePin(phase, gpio_port, gpio_pin, alt_func);

// Set commutation mode
void setMode(LQ_BLDC_MODE_6STEP);    // Trapezoidal
void setMode(LQ_BLDC_MODE_SINE);     // Sinusoidal (smooth)
void setMode(LQ_BLDC_MODE_FOC);      // Field-oriented
void setMode(LQ_BLDC_MODE_OPEN_LOOP); // V/f control
```

### Control

```cpp
void enable(true);              // Enable motor
void setPower(75);              // Set power 0-100
void setDirection(true);        // true=forward, false=reverse
void emergencyStop();           // Immediate brake
void update();                  // Update commutation (call in loop!)
```

### Status

```cpp
uint8_t power = motor.getPower();
bool enabled = motor.isEnabled();
```

## Parameter Tuning

### Pole Pairs

Check your motor datasheet:
```cpp
// Common values:
// - Hobby motors: 7, 14 pole pairs
// - Industrial: 2-8 pole pairs
// - High-speed: 1-4 pole pairs
motor.begin(3, 7, ...);  // 7 pole pairs
```

### PWM Frequency

```cpp
// Lower frequency = less switching loss, more audible noise
motor.begin(3, 7, 20000, ...);  // 20 kHz

// Higher frequency = less noise, more switching loss
motor.begin(3, 7, 40000, ...);  // 40 kHz

// Recommended: 25 kHz
motor.begin(3, 7, 25000, ...);  // 25 kHz (good balance)
```

### Deadtime

```cpp
// Prevents shoot-through in half-bridge
// Too little = shoot-through (MOSFETs can die!)
// Too much = reduced output voltage

// Small MOSFETs (TO-220)
motor.begin(3, 7, 25000, 500);   // 500 ns

// Medium MOSFETs (D2PAK)
motor.begin(3, 7, 25000, 1000);  // 1 μs (recommended)

// Large MOSFETs (>100A)
motor.begin(3, 7, 25000, 2000);  // 2 μs
```

## Troubleshooting

### "Motor init failed"

**Cause**: Platform not supported or wrong board selected

**Fix**:
- Check board selection in Arduino IDE
- Verify you're using SAMD21/51 or ESP32
- Check pin configuration matches your board

### Motor doesn't spin

**Causes**:
1. No power to motor driver
2. Wrong pin configuration
3. Motor not enabled

**Fixes**:
```cpp
// Add debug output
if (!motor.begin(...)) {
  Serial.println("Init failed");
  while(1);
}
Serial.println("Motor ready");
motor.enable(true);
Serial.println("Motor enabled");
```

### Motor stutters or vibrates

**Causes**:
- Wrong pole pairs
- PWM frequency too low
- Not calling `update()` regularly

**Fixes**:
```cpp
// Check pole pairs (try different values)
motor.begin(3, 14, ...);  // Try 14 instead of 7

// Increase PWM frequency
motor.begin(3, 7, 40000, ...);

// Ensure update() is called
void loop() {
  motor.update();  // MUST call every ~1ms
  // ...
  delay(1);
}
```

### Motor overheats

**Causes**:
- Too high power setting
- No deadtime (shoot-through!)
- Wrong motor for voltage

**Fixes**:
```cpp
// Reduce max power
motor.setPower(50);  // Start low

// Check deadtime is enabled
motor.begin(3, 7, 25000, 1000);  // 1μs deadtime

// Use proper motor driver with current limiting
```

## Differences from DTS-Based Approach

### Main Repository (DTS-Based)

```dts
/* Device tree file */
bldc_motor: bldc@0 {
    compatible = "lq,bldc-motor";
    num-phases = <3>;
    pole-pairs = <7>;
    high-side-pins = <0 4 0x04  0 5 0x04  0 6 0x04>;
    low-side-pins = <0 10 0x04  0 11 0x04  0 12 0x04>;
};
```

```bash
# Generate code
python scripts/dts_gen.py config.dts output/
```

### Arduino Library (Direct API)

```cpp
/* Equivalent Arduino code */
motor.setHighSidePin(0, 0, 4, 0x04);
motor.setHighSidePin(1, 0, 5, 0x04);
motor.setHighSidePin(2, 0, 6, 0x04);

motor.setLowSidePin(0, 0, 10, 0x04);
motor.setLowSidePin(1, 0, 11, 0x04);
motor.setLowSidePin(2, 0, 12, 0x04);

motor.begin(3, 7, 25000, 1000);
```

**Pros of Arduino approach**:
- No external tooling needed
- Works in Arduino IDE out-of-box
- Easier for beginners
- Standard Arduino workflow

**Pros of DTS approach**:
- Compile-time configuration validation
- Easier to manage complex systems
- Better for production/embedded
- Integration with Zephyr RTOS

## Next Steps

1. **Run examples**: Try `SAMD_BLDC_Simple` or `ESP32_BLDC_Simple`
2. **Connect hardware**: Wire motor driver to Arduino
3. **Test safely**: Start with low voltage and secure motor
4. **Tune parameters**: Adjust pole pairs, PWM frequency, deadtime
5. **Add sensors**: Integrate encoders, current sensing, etc.

## Support

- **Examples**: See `arduino/examples/` folder
- **Documentation**: Check `docs/bldc-motor-driver.md` in main repo
- **Issues**: Report on GitHub
- **Hardware**: Test with low voltage first!

## Safety

⚠️ **READ THIS BEFORE USING**

1. **Secure the motor** - it WILL spin fast
2. **Use deadtime** - prevents MOSFET damage
3. **Test at 5V first** - before using 12V/24V
4. **Add current limiting** - in hardware
5. **Emergency stop** - always have a way to kill power
6. **Never touch** - while motor is running

## License

Apache-2.0
