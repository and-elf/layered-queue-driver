# BLDC Motor Driver Guide

## Overview

The Layered Queue BLDC (Brushless DC) motor driver provides a platform-independent interface for controlling n-phase BLDC motors with various commutation strategies. The driver handles electrical angle calculation, phase commutation, and synchronized PWM generation.

## Features

- **N-Phase Support**: Configurable from 1-6 phases (3-phase most common)
- **Multiple Commutation Modes**:
  - 6-step (trapezoidal) - Simple, efficient
  - Sinusoidal PWM (SPWM) - Smooth, quiet operation
  - Field-Oriented Control (FOC) - Maximum efficiency
  - Open-loop V/f - Speed control without sensors
- **Power Control**: 0-100 input range with configurable resolution
- **Direction Control**: Forward/reverse operation
- **Safety Features**:
  - Emergency stop with active braking
  - Deadtime insertion for complementary PWM
  - Platform-specific safety (brake pins, current limits)
- **Platform Abstraction**: Works on STM32, ESP32, SAMD21/51, etc.

## Supported Platforms

| Platform | Peripheral | Complementary PWM | Deadtime | Max Motors | Status |
|----------|------------|-------------------|----------|------------|--------|
| STM32F4/F7/H7 | TIM1/TIM8 | ✅ | ✅ | Multiple | ✅ Complete |
| ESP32 | MCPWM | ✅ | ✅ | 2 | ✅ Complete |
| ESP32-S3 | MCPWM | ✅ | ✅ | 1 | ✅ Complete |
| SAMD21 | TCC0 | ✅ | ✅ | 1 | ✅ Complete |
| SAMD51 | TCC0/TCC1 | ✅ | ✅ | 2 | ✅ Complete |
| Nordic nRF52 | PWM | ❌ | ❌ | - | ⚠️ Requires external logic |

See platform-specific examples in [samples/](../samples/).

## Architecture

```
Application
    ↓
lq_bldc.h (Driver API)
    ↓
lq_bldc.c (Commutation Logic)
    ↓
lq_platform.h (Platform Abstraction)
    ↓
Platform Implementation (STM32/ESP32/etc)
    ↓
Hardware PWM Timers
```

## Motor Configuration

### Device Tree Example

```dts
bldc_motor: bldc@0 {
    compatible = "lq,bldc-motor";
    label = "MAIN_MOTOR";
    motor-id = <0>;
    
    /* Electrical configuration */
    num-phases = <3>;
    pole-pairs = <7>;
    
    /* Commutation strategy */
    mode = "sine";  /* "6step", "sine", "foc", "open-loop" */
    
    /* PWM timing */
    pwm-frequency-hz = <25000>;
    max-duty-cycle = <9500>;  /* 95% in 0.01% units */
    
    /* Safety */
    enable-deadtime;
    deadtime-ns = <1000>;
    
    /* STM32 TIM1 Pin Configuration */
    /* High-side pins: [port, pin, AF] */
    high-side-pins = <
        0  8  1   /* PA8  = TIM1_CH1  (Phase U) */
        0  9  1   /* PA9  = TIM1_CH2  (Phase V) */
        0 10  1   /* PA10 = TIM1_CH3  (Phase W) */
    >;
    
    /* Low-side (complementary) pins: [port, pin, AF] */
    low-side-pins = <
        0  7  1   /* PA7  = TIM1_CH1N (Phase U complementary) */
        1  0  1   /* PB0  = TIM1_CH2N (Phase V complementary) */
        1  1  1   /* PB1  = TIM1_CH3N (Phase W complementary) */
    >;
    
    status = "okay";
};
```

### Configuration Structure

```c
struct lq_bldc_pin {
    uint8_t gpio_port;           /* GPIO port (0=GPIOA, 1=GPIOB, ...) */
    uint8_t gpio_pin;            /* Pin number */
    uint8_t alternate_function;  /* Timer alternate function */
};

struct lq_bldc_config {
    uint8_t num_phases;          /* 1-6 phases */
    uint8_t pole_pairs;          /* Motor pole pairs */
    enum lq_bldc_mode mode;      /* Commutation mode */
    uint32_t pwm_frequency_hz;   /* PWM frequency */
    uint16_t max_duty_cycle;     /* Max duty in 0.01% (0-10000) */
    bool enable_deadtime;        /* Enable deadtime insertion */
    uint16_t deadtime_ns;        /* Deadtime in nanoseconds */
    
    /* Pin configuration (platform-specific) */
    struct lq_bldc_pin high_side_pins[LQ_BLDC_MAX_PHASES];
    struct lq_bldc_pin low_side_pins[LQ_BLDC_MAX_PHASES];
};
```

### Platform Capability Detection

The driver includes compile-time checks for platform capabilities:

```c
/* Defined in lq_platform.h */
#if defined(STM32F4) || defined(STM32F7) || defined(STM32H7) || defined(STM32G4)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 1
    #define LQ_PLATFORM_HAS_DEADTIME 1
#elif defined(ESP32) || defined(ESP32S3)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 1
    #define LQ_PLATFORM_HAS_DEADTIME 1
#elif defined(NRF52) || defined(NRF53)
    #define LQ_PLATFORM_HAS_COMPLEMENTARY_PWM 0
    #define LQ_PLATFORM_HAS_DEADTIME 0
#endif
```

**Platforms with complementary PWM:**
- STM32F4/F7/H7/G4 (Advanced timers TIM1/TIM8)
- ESP32/ESP32-S3 (MCPWM peripheral)

**Platforms without complementary PWM:**
- Nordic nRF52/nRF53 (requires external gate driver logic)
- Native/POSIX (stub implementation for testing)

If you try to use deadtime on an unsupported platform, you'll get a compile-time error:
```
error: Platform does not support complementary PWM channels required for BLDC control
```

## API Usage

### Initialization

```c
#include "lq_bldc.h"

struct lq_bldc_motor motor;

void motor_init(void) {
    struct lq_bldc_config config = {
        .num_phases = 3,
        .pole_pairs = 7,
        .mode = LQ_BLDC_MODE_SINE,
        .pwm_frequency_hz = 25000,
        .max_duty_cycle = 9500,
        .enable_deadtime = true,
        .deadtime_ns = 1000,
        
        /* STM32 TIM1 pin configuration */
        .high_side_pins = {
            {.gpio_port = 0, .gpio_pin = 8,  .alternate_function = 1},  /* PA8  */
            {.gpio_port = 0, .gpio_pin = 9,  .alternate_function = 1},  /* PA9  */
            {.gpio_port = 0, .gpio_pin = 10, .alternate_function = 1},  /* PA10 */
        },
        .low_side_pins = {
            {.gpio_port = 0, .gpio_pin = 7, .alternate_function = 1},   /* PA7 */
            {.gpio_port = 1, .gpio_pin = 0, .alternate_function = 1},   /* PB0 */
            {.gpio_port = 1, .gpio_pin = 1, .alternate_function = 1},   /* PB1 */
        },
    };
    
    int ret = lq_bldc_init(&motor, &config, 0);
    if (ret != 0) {
        /* Handle error */
    }
}
```

### Power Control

```c
void set_throttle(uint8_t throttle) {
    /* throttle: 0-100 */
    lq_bldc_set_power(&motor, throttle);
    
    /* Enable motor if throttle > 0 */
    if (throttle > 0) {
        lq_bldc_enable(&motor, true);
    } else {
        lq_bldc_enable(&motor, false);
    }
}
```

### Direction Control

```c
void set_direction(bool reverse) {
    enum lq_bldc_direction dir = reverse ? 
        LQ_BLDC_DIR_REVERSE : LQ_BLDC_DIR_FORWARD;
    lq_bldc_set_direction(&motor, dir);
}
```

### Update Loop

```c
void motor_task(void) {
    static uint32_t last_update = 0;
    uint32_t now = lq_platform_uptime_get();
    
    if (now - last_update >= 1) {  /* 1ms update */
        uint32_t delta_us = (now - last_update) * 1000;
        lq_bldc_update(&motor, delta_us);
        last_update = now;
    }
}
```

### Emergency Stop

```c
void emergency_stop(void) {
    lq_bldc_emergency_stop(&motor);
    /* Motor is now disabled with active braking */
}
```

## Commutation Modes

### 6-Step Commutation

**Best for**: Cost-sensitive applications, high efficiency at high speeds

```c
config.mode = LQ_BLDC_MODE_6STEP;
```

**Characteristics**:
- Trapezoidal back-EMF motors
- 60° conduction per phase
- Simple commutation pattern
- Audible switching noise
- ~85-90% efficiency

**Commutation Pattern** (3-phase):
```
Step | Phase A | Phase B | Phase C
-----|---------|---------|--------
  0  |   +PWM  |    -    |   OFF
  1  |   +PWM  |   OFF   |    -
  2  |   OFF   |   +PWM  |    -
  3  |    -    |   +PWM  |   OFF
  4  |    -    |   OFF   |   +PWM
  5  |   OFF   |    -    |   +PWM
```

### Sinusoidal PWM (SPWM)

**Best for**: Quiet operation, smooth torque

```c
config.mode = LQ_BLDC_MODE_SINE;
```

**Characteristics**:
- Sine wave modulation
- Continuous conduction (all 3 phases active)
- Low audible noise
- Smooth torque ripple
- ~80-85% efficiency

**Waveforms**:
```
Phase A: sin(θ)
Phase B: sin(θ - 120°)
Phase C: sin(θ - 240°)
```

### Field-Oriented Control (FOC)

**Best for**: Maximum efficiency, precise control

```c
config.mode = LQ_BLDC_MODE_FOC;
```

**Characteristics**:
- Clarke/Park transforms
- Requires current sensing
- Torque and flux control
- Best efficiency (~90-95%)
- Complex implementation

**Note**: Full FOC requires current sensors and PID loops (future enhancement).

### Open-Loop V/f

**Best for**: Sensorless startup, variable speed

```c
config.mode = LQ_BLDC_MODE_OPEN_LOOP;
```

**Characteristics**:
- Voltage proportional to frequency
- No position feedback required
- Simple speed control
- Poor low-speed torque

## Platform Implementation

### Required Platform Functions

```c
/* Initialize PWM hardware with pin configuration */
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config);

/* Set duty cycle for a phase */
int lq_bldc_platform_set_duty(uint8_t motor_id,
                               uint8_t phase,
                               uint16_t duty_cycle);

/* Enable/disable PWM outputs */
int lq_bldc_platform_enable(uint8_t motor_id, bool enable);

/* Emergency brake */
int lq_bldc_platform_brake(uint8_t motor_id);
```

### Platform Comparison

| Feature | STM32F4/F7/H7 | ESP32 | ESP32-S3 | SAMD21/SAMD51 | Nordic nRF52 |
|---------|---------------|-------|----------|---------------|--------------|
| Complementary PWM | ✅ TIM1/TIM8 | ✅ MCPWM | ✅ MCPWM | ✅ TCC | ❌ |
| Hardware Deadtime | ✅ Yes | ✅ Yes | ✅ Yes | ✅ Yes | ❌ |
| Max Motors | Multiple | 2 | 1 | 2 (SAMD51) | - |
| Center-Aligned | ✅ Yes | ✅ Yes | ✅ Yes | ⚠️ NPWM | - |
| Break Input | ✅ Yes | ⚠️ GPIO | ⚠️ GPIO | ⚠️ Fault | - |
| Resolution | 16-bit | 16-bit | 16-bit | 24-bit | - |

### STM32 Example

See [src/platform/lq_platform_stm32.c](../src/platform/lq_platform_stm32.c) for a complete STM32F4 implementation using TIM1 with complementary outputs.

**Key features**:
- TIM1 advanced timer with deadtime generation
- Center-aligned PWM for symmetric waveforms
- Complementary outputs (TIMx_CHy/TIMx_CHyN)
- Break input for safety
- Configurable pins via config struct

**Pin Configuration**:
The platform implementation reads pin configuration from the config struct:
```c
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
    /* Initialize timer */
    tim1_pwm_init(config->pwm_frequency_hz, config->deadtime_ns);
    
    /* Configure GPIO pins from config */
    for (uint8_t phase = 0; phase < config->num_phases; phase++) {
        const struct lq_bldc_pin *hs_pin = &config->high_side_pins[phase];
        const struct lq_bldc_pin *ls_pin = &config->low_side_pins[phase];
        
        /* Map port number to GPIO base */
        GPIO_TypeDef *hs_port = (GPIO_TypeDef *)(GPIOA_BASE + (hs_pin->gpio_port * 0x400));
        
        /* Configure as alternate function */
        GPIO_InitStruct.Pin = (1U << hs_pin->gpio_pin);
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Alternate = hs_pin->alternate_function;
        HAL_GPIO_Init(hs_port, &GPIO_InitStruct);
        /* ... */
    }
}
```

### ESP32 Example

ESP32 MCPWM peripheral supports complementary PWM and deadtime:

**Hardware capabilities**:
- **MCPWM0 and MCPWM1** units (ESP32 has 2, ESP32-S3 has 1)
- Each unit has **3 timers** (one per phase)
- **Complementary outputs** (PWM_A/PWM_B pairs)
- **Configurable deadtime** (in 160MHz clock cycles)
- **Center-aligned PWM** mode

**Pin Configuration**:
ESP32 allows flexible pin mapping. Common configuration:

| Phase | High-Side (A) | Low-Side (B) |
|-------|---------------|--------------|
| U     | GPIO16 (MCPWM0A) | GPIO17 (MCPWM0B) |
| V     | GPIO18 (MCPWM1A) | GPIO5 (MCPWM1B) |
| W     | GPIO19 (MCPWM2A) | GPIO4 (MCPWM2B) |

**Platform Implementation** ([src/platform/lq_platform_esp32.c](../src/platform/lq_platform_esp32.c)):

```c
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
    mcpwm_unit_t unit = (motor_id == 0) ? MCPWM_UNIT_0 : MCPWM_UNIT_1;
    
    /* Configure GPIO pins from config */
    for (uint8_t phase = 0; phase < 3; phase++) {
        int hs_gpio = (hs_pin->gpio_port * 32) + hs_pin->gpio_pin;
        int ls_gpio = (ls_pin->gpio_port * 32) + ls_pin->gpio_pin;
        
        mcpwm_gpio_init(unit, MCPWM0A + phase*2, hs_gpio);
        mcpwm_gpio_init(unit, MCPWM0B + phase*2, ls_gpio);
    }
    
    /* Configure MCPWM with center-aligned mode */
    mcpwm_config_t pwm_config = {
        .frequency = config->pwm_frequency_hz,
        .counter_mode = MCPWM_UP_DOWN_COUNTER,  /* Center-aligned */
    };
    mcpwm_init(unit, timer, &pwm_config);
    
    /* Enable deadtime */
    if (config->enable_deadtime) {
        uint32_t dt_cycles = (config->deadtime_ns * 160) / 1000;
        mcpwm_deadtime_enable(unit, timer, 
                             MCPWM_ACTIVE_HIGH_COMPLIMENT_MODE,
                             dt_cycles, dt_cycles);
    }
}
```

**Multi-Motor Support**:
- ESP32: Up to **2 motors** (MCPWM_UNIT_0, MCPWM_UNIT_1)
- ESP32-S3: Up to **1 motor** (MCPWM_UNIT_0 only)

See [samples/esp32/bldc_motor_example.c](../samples/esp32/bldc_motor_example.c) for complete example with FreeRTOS integration.

### SAMD Example

SAMD21/SAMD51 microcontrollers have TCC (Timer/Counter for Control) modules with excellent motor control features:

**Hardware capabilities**:
- **TCC0, TCC1, TCC2** modules (SAMD21: 3 TCC, SAMD51: 5 TCC)
- Each TCC has **4 compare channels** (up to 8 waveform outputs)
- **Complementary outputs** via pattern generation
- **Hardware deadtime insertion** (up to 255 clock cycles)
- **24-bit resolution** (smoother PWM than 16-bit)
- **Fault detection** with recoverable states

**Pin Configuration**:
SAMD TCC outputs are mapped through the PORT peripheral mux:

| Phase | High-Side (WO) | Low-Side (WO) | SAMD21 Pin | SAMD51 Pin |
|-------|----------------|---------------|------------|------------|
| U     | TCC0/WO[0]     | TCC0/WO[4]    | PA04/PA10  | PB12/PA14  |
| V     | TCC0/WO[1]     | TCC0/WO[5]    | PA05/PA11  | PB13/PA15  |
| W     | TCC0/WO[2]     | TCC0/WO[6]    | PA06/PA12  | PA20/PA16  |

**Platform Implementation** ([src/platform/lq_platform_samd.c](../src/platform/lq_platform_samd.c)):

```c
int lq_bldc_platform_init(uint8_t motor_id, const struct lq_bldc_config *config) {
    Tcc *tcc = (motor_id == 0) ? TCC0 : TCC1;
    
    /* Configure GPIO pins from config */
    for (uint8_t phase = 0; phase < 3; phase++) {
        /* High-side pin: PA04 = TCC0/WO[0], function E (0x04) */
        Port *port = (config->high_side_pins[phase].gpio_port == 0) ? PORT : PORTB;
        uint8_t pin = config->high_side_pins[phase].gpio_pin;
        uint8_t func = config->high_side_pins[phase].alternate_function;
        
        port->Group[0].PINCFG[pin].bit.PMUXEN = 1;
        if (pin & 1) {
            port->Group[0].PMUX[pin >> 1].bit.PMUXO = func;
        } else {
            port->Group[0].PMUX[pin >> 1].bit.PMUXE = func;
        }
    }
    
    /* Calculate period for PWM frequency */
    uint32_t tcc_clock = 48000000UL;  /* 48 MHz on SAMD21 */
    uint32_t period = (tcc_clock / config->pwm_frequency_hz) - 1;
    
    /* Configure TCC waveform generation */
    tcc->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;  /* Normal PWM */
    tcc->PER.reg = period;
    
    /* Enable deadtime for complementary outputs */
    if (config->enable_deadtime) {
        uint32_t dt_cycles = (config->deadtime_ns * (tcc_clock / 1000000)) / 1000;
        tcc->WEXCTRL.reg = TCC_WEXCTRL_DTIEN0 | TCC_WEXCTRL_DTIEN1 | TCC_WEXCTRL_DTIEN2 |
                          TCC_WEXCTRL_DTLS(dt_cycles) | TCC_WEXCTRL_DTHS(dt_cycles);
    }
    
    /* Enable TCC */
    tcc->CTRLA.bit.ENABLE = 1;
}
```

**Multi-Motor Support**:
- **SAMD21**: Up to **1 motor** (TCC0 recommended, 3 channels)
- **SAMD51**: Up to **2 motors** (TCC0 + TCC1, each with 4 channels)

**Arduino Integration**:
SAMD boards (Arduino Zero, MKR series, Adafruit Feather M0/M4) can use this driver:

```cpp
void setup() {
    bldc_example_init();  /* Initialize motor on TCC0 */
    lq_bldc_enable(&motor, true);
}

void loop() {
    bldc_example_control(throttle);  /* Update motor control */
    delay(1);  /* 1ms update rate */
}
```

See [samples/samd/bldc_motor_example.c](../samples/samd/bldc_motor_example.c) for complete Arduino-compatible example.

## Multi-Motor Control

For applications requiring multiple motors (drones, robotics):

```c
struct lq_bldc_motor motors[4];  /* Quad-rotor drone */

void init_all_motors(void) {
    struct lq_bldc_config config = {
        .num_phases = 3,
        .pole_pairs = 14,  /* High-KV motors */
        .mode = LQ_BLDC_MODE_SINE,
        .pwm_frequency_hz = 32000,
        .max_duty_cycle = 9800,
        .enable_deadtime = true,
        .deadtime_ns = 800
    };
    
    for (int i = 0; i < 4; i++) {
        lq_bldc_init(&motors[i], &config, i);
        lq_bldc_enable(&motors[i], true);
    }
}

void update_all_motors(uint8_t throttle[4]) {
    uint32_t delta_us = get_delta_time();
    
    for (int i = 0; i < 4; i++) {
        lq_bldc_set_power(&motors[i], throttle[i]);
        lq_bldc_update(&motors[i], delta_us);
    }
}
```

## Safety Considerations

### Deadtime Insertion

**Critical for preventing shoot-through** in complementary PWM:

```
High-side:  ___/‾‾‾‾‾‾‾‾‾‾‾\_____
                 |<-dt->|
Low-side:   ‾‾‾\_________/‾‾‾‾‾‾‾
```

**Typical values**:
- Low-voltage (12V): 500-1000ns
- Mid-voltage (48V): 1000-2000ns
- High-voltage (>100V): 2000-4000ns

### Emergency Stop

```c
/* Hardware fault interrupt */
void EXTI_IRQHandler(void) {
    lq_bldc_emergency_stop(&motor);
    /* Active braking applied */
}
```

### Current Limiting

Platform implementation should monitor phase currents:

```c
int lq_bldc_platform_set_duty(uint8_t motor_id, uint8_t phase,
                               uint16_t duty_cycle) {
    /* Check current limit before applying duty cycle */
    if (get_phase_current(motor_id, phase) > MAX_CURRENT) {
        lq_bldc_emergency_stop(&motor);
        return -1;
    }
    
    /* Safe to apply */
    set_timer_duty(motor_id, phase, duty_cycle);
    return 0;
}
```

## Performance Tuning

### PWM Frequency Selection

| Application | Frequency | Reason |
|-------------|-----------|--------|
| Low-power motors | 16-20 kHz | Audible range, efficiency |
| Mid-power motors | 20-40 kHz | Ultrasonic, low noise |
| High-power motors | 8-16 kHz | Switching losses |

### Update Rate

- **Minimum**: 100 Hz (10ms) for basic control
- **Recommended**: 1 kHz (1ms) for smooth operation
- **High-performance**: 10 kHz (100μs) for FOC

### Electrical Angle Calculation

Current implementation uses open-loop estimation:

```c
/* In lq_bldc_update() */
uint32_t rpm_target = (motor->state.power * 3000) / 100;
uint32_t angle_increment = (rpm_target * 65536UL * delta_time_us) / 
                          (60UL * 1000000UL);
motor->state.electrical_angle += angle_increment;
```

For sensorless back-EMF sensing (future):

```c
/* Read zero-crossing on floating phase */
uint16_t angle = detect_zero_crossing(motor_id);
motor->state.electrical_angle = angle;
```

## Integration with Layered Queue

### As Queue Output

BLDC motors can be controlled via the queue system:

```c
/* In application code */
struct lq_event motor_event = {
    .type = LQ_OUTPUT_BLDC,
    .value = throttle_percent,  /* 0-100 */
    .timestamp = lq_platform_uptime_get()
};

lq_queue_push(&motor_queue, &motor_event);

/* In generated dispatch */
case LQ_OUTPUT_BLDC:
    lq_bldc_set_power(&motor, event->value);
    break;
```

### With Fault Monitoring

```c
/* Monitor motor faults */
if (motor.state.mechanical_rpm < expected_rpm * 0.8) {
    /* Motor stall detected */
    lq_dtc_set_fault(DTC_MOTOR_STALL);
    lq_bldc_emergency_stop(&motor);
}
```

## Testing

Unit tests for commutation algorithms:

```cpp
TEST(BLDCTest, SixStepCommutation) {
    struct lq_bldc_motor motor;
    struct lq_bldc_config config = {
        .num_phases = 3,
        .mode = LQ_BLDC_MODE_6STEP,
        .max_duty_cycle = 10000
    };
    
    lq_bldc_init(&motor, &config, 0);
    lq_bldc_set_power(&motor, 50);  /* 50% power */
    lq_bldc_update(&motor, 1000);   /* 1ms update */
    
    /* Verify commutation pattern */
    EXPECT_GT(motor.state.duty_cycle[0], 0);
    /* ... */
}
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Motor won't spin | Wrong phase sequence | Swap two phases |
| Excessive vibration | Incorrect pole pairs | Verify motor specs |
| Overheating | Too high PWM frequency | Reduce to 20 kHz |
| Shoot-through | Insufficient deadtime | Increase deadtime |
| Poor low-speed torque | Open-loop control | Add current sensing |

## References

- AN2606: "BLDC Motor Control with STM32"
- AN1160: "Sensorless BLDC Control"
- [samples/stm32/bldc_motor_example.c](../samples/stm32/bldc_motor_example.c)
- [samples/stm32/bldc-motor-example.dts](../samples/stm32/bldc-motor-example.dts)
