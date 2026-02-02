# Using the BLDC Motor Driver

This repository provides a platform-independent BLDC motor driver that can be used in two ways:

## 1. Arduino Library (Easiest - No Code Generation)

**Best for**: Arduino users, beginners, quick prototyping

The `arduino/` folder contains a complete Arduino library that works directly in the Arduino IDE without needing device tree files or code generation.

### Quick Start

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  motor.setHighSidePin(0, 0, 4, 0x04);   // Configure pins
  motor.setHighSidePin(1, 0, 5, 0x04);
  motor.setHighSidePin(2, 0, 6, 0x04);
  
  motor.setLowSidePin(0, 0, 10, 0x04);
  motor.setLowSidePin(1, 0, 11, 0x04);
  motor.setLowSidePin(2, 0, 12, 0x04);
  
  motor.begin(3, 7, 25000, 1000);  // 3-phase, 7 pole pairs, 25kHz, 1μs deadtime
  motor.enable(true);
}

void loop() {
  motor.update();
  motor.setPower(50);  // 50% power
  delay(1);
}
```

### Installation

**Option 1**: Arduino Library Manager
1. Open Arduino IDE
2. Tools → Manage Libraries
3. Search "LayeredQueue BLDC"
4. Install

**Option 2**: Manual
1. Copy the `arduino/` folder to `~/Arduino/libraries/LayeredQueue_BLDC/`
2. Restart Arduino IDE

### Documentation

- **Getting Started**: [arduino/GETTING_STARTED.md](arduino/GETTING_STARTED.md)
- **API Reference**: [arduino/README.md](arduino/README.md)
- **Examples**: See `arduino/examples/`

### Supported Boards

- Arduino Zero, MKR series (SAMD21)
- Adafruit Metro M4, Grand Central (SAMD51)
- ESP32, ESP32-S3
- STM32 with Arduino core

---

## 2. Full System (Advanced - With Code Generation)

**Best for**: Production systems, Zephyr RTOS, complex configurations

The main repository uses device tree files and code generation for complete embedded systems.

### Quick Start

1. **Create device tree configuration**:

```dts
/* config.dts */
/ {
    motor0: bldc-motor@0 {
        compatible = "lq,bldc-motor";
        motor-id = <0>;
        num-phases = <3>;
        pole-pairs = <7>;
        pwm-frequency-hz = <25000>;
        mode = "sine";
        deadtime-ns = <1000>;
        
        high-side-pins = <0 4 0x04  0 5 0x04  0 6 0x04>;
        low-side-pins = <0 10 0x04  0 11 0x04  0 12 0x04>;
    };
};
```

2. **Generate code**:

```bash
python scripts/dts_gen.py config.dts output/ --platform=samd
```

3. **Use generated code**:

```c
#include "lq_generated.h"

int main(void) {
    lq_generated_init();
    
    while (1) {
        lq_engine_step(&g_lq_engine, timestamp);
        lq_generated_dispatch_outputs();
        delay_ms(1);
    }
}
```

### Documentation

- **Architecture**: [docs/bldc-motor-driver.md](docs/bldc-motor-driver.md)
- **Platform Guide**: [docs/platform-adaptors.md](docs/platform-adaptors.md)
- **DTS Guide**: [docs/devicetree-guide.md](docs/devicetree-guide.md)

### Supported Platforms

- Zephyr RTOS (native integration)
- FreeRTOS
- Bare metal (STM32, ESP32, SAMD)
- Native (testing)

---

## Comparison

| Feature | Arduino Library | Full System |
|---------|----------------|-------------|
| **Setup** | Arduino IDE only | Python, build tools |
| **Configuration** | C++ API calls | Device tree files |
| **Code Generation** | Not needed | Python scripts |
| **Complexity** | Simple | Advanced |
| **Best For** | Hobbyists, prototypes | Production, RTOS |
| **Platform Support** | Arduino-compatible | Any embedded |
| **Learning Curve** | Low | Medium-High |

## Platform Support Matrix

| Platform | Arduino Library | Full System | Complementary PWM | Deadtime |
|----------|----------------|-------------|-------------------|----------|
| SAMD21 | ✅ | ✅ | ✅ TCC0 | ✅ |
| SAMD51 | ✅ | ✅ | ✅ TCC0/1 | ✅ |
| ESP32 | ✅ | ✅ | ✅ MCPWM | ✅ |
| ESP32-S3 | ✅ | ✅ | ✅ MCPWM | ✅ |
| STM32F4/F7/H7 | ⚠️ | ✅ | ✅ TIM1/8 | ✅ |
| Nordic nRF52 | ❌ | ⚠️ | ❌ | ❌ |

**Legend**: ✅ Full support | ⚠️ Partial support | ❌ Not supported

## Which Should I Use?

### Use Arduino Library if:
- ✅ You're using Arduino IDE
- ✅ You want a simple, direct API
- ✅ You're prototyping or learning
- ✅ You have a single motor to control
- ✅ You don't need complex signal processing

### Use Full System if:
- ✅ You're building a production system
- ✅ You need multiple coordinated motors
- ✅ You're using Zephyr or FreeRTOS
- ✅ You need advanced features (fault monitoring, J1939, etc.)
- ✅ You want compile-time validation

## Examples

### Arduino Example (Simple)

```cpp
#include <LayeredQueue_BLDC.h>

BLDC_Motor motor(0);

void setup() {
  motor.setHighSidePin(0, 0, 16, 0);  // ESP32 GPIO16
  motor.setHighSidePin(1, 0, 18, 0);  // ESP32 GPIO18
  motor.setHighSidePin(2, 0, 19, 0);  // ESP32 GPIO19
  
  motor.setLowSidePin(0, 0, 17, 0);
  motor.setLowSidePin(1, 0, 5, 0);
  motor.setLowSidePin(2, 0, 4, 0);
  
  motor.begin();
  motor.enable(true);
}

void loop() {
  motor.update();
  motor.setPower(analogRead(A0) / 40);  // Pot control
  delay(1);
}
```

### Full System Example (Advanced)

```c
#include "lq_bldc.h"

static struct lq_bldc_motor motor;

int main(void) {
    struct lq_bldc_config config = {
        .num_phases = 3,
        .pole_pairs = 7,
        /* ... pin configuration ... */
    };
    
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_enable(&motor, true);
    
    while (1) {
        uint32_t delta_us = get_delta_time();
        lq_bldc_update(&motor, delta_us);
        k_sleep(K_MSEC(1));
    }
}
```

## Building

### Arduino (No Build Required)

Just open the `.ino` file in Arduino IDE and click Upload.

### Full System

```bash
# CMake build
mkdir build && cd build
cmake ..
make

# Or with tests
cmake .. -DBUILD_TESTING=ON
make
ctest
```

## Contributing

Both approaches share the same core BLDC driver code (`src/drivers/lq_bldc.c`), so improvements benefit both users.

## License

Apache-2.0

## Support

- **Arduino Questions**: See [arduino/GETTING_STARTED.md](arduino/GETTING_STARTED.md)
- **Full System**: See [docs/](docs/)
- **Issues**: GitHub Issues
- **Hardware Help**: Check safety guidelines first!
