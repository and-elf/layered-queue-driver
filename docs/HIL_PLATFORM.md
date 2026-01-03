# HIL Platform Layer

## Overview

The HIL (Hardware-in-the-Loop) platform layer enables running the **actual generated application** in HIL mode without modification. All hardware access (ADC, CAN, GPIO) is automatically intercepted and routed through HIL messages.

## Architecture

```
┌──────────────────────────────────────┐
│  Generated Application Code          │
│  (lq_generated.c)                    │
│  ┌────────────────────────────────┐  │
│  │ lq_generated_init()            │  │
│  │ lq_engine_start()              │  │
│  └────────────────────────────────┘  │
│             ↓                         │
│  ┌────────────────────────────────┐  │
│  │ Platform API (lq_platform.h)   │  │
│  │ - lq_adc_read()                │  │
│  │ - lq_can_send()                │  │
│  │ - lq_gpio_set()                │  │
│  └────────────────────────────────┘  │
└──────────────│───────────────────────┘
               │
      ┌────────┴─────────┐
      │  Runtime Mode     │
      └────────┬─────────┘
               │
      ┌────────┴──────────────────────┐
      │                               │
┌─────▼─────────┐        ┌────────────▼────────┐
│ Native Mode   │        │   HIL Mode          │
│ (Real HW)     │        │   (Test Messages)   │
│               │        │                     │
│ Read ADC reg  │        │ lq_hil_sut_recv()   │
│ Write CAN bus │        │ lq_hil_sut_send()   │
│ Set GPIO pin  │        │ Unix socket IPC     │
└───────────────┘        └──────────┬──────────┘
                                    │
                         ┌──────────▼──────────┐
                         │  HIL Tester Process │
                         │  (test_runner)      │
                         │                     │
                         │  inject-adc         │
                         │  expect-can         │
                         │  measure-latency    │
                         └─────────────────────┘
```

## How It Works

### 1. Platform Abstraction

All hardware access goes through [lq_platform.h](../include/lq_platform.h):

```c
// Your generated code calls:
int lq_adc_read(uint8_t channel, uint16_t *value);
int lq_can_send(uint32_t can_id, bool is_extended, 
                const uint8_t *data, uint8_t len);
int lq_gpio_set(uint8_t pin, bool value);
```

### 2. Platform Implementation Selection

**Native Mode** ([lq_platform_native.c](../src/platform/lq_platform_native.c)):
- Accesses real hardware
- Reads actual ADC channels
- Transmits on real CAN bus
- Controls physical GPIO pins

**HIL Mode** ([lq_platform_hil.c](../src/platform/lq_platform_hil.c)):
- Routes through HIL messages
- Receives ADC values from tester
- Sends CAN messages to tester for verification
- Notifies tester of GPIO changes

### 3. Automatic Mode Detection

The HIL platform automatically detects mode via environment variable:

```bash
# Native mode (default)
./my_app

# HIL mode (SUT side)
LQ_HIL_MODE=sut ./my_app

# HIL mode (tester side)
LQ_HIL_MODE=tester ./my_test
```

## Implementation Details

### Platform Layer Functions

**ADC Read** - Receives injected values from tester:
```c
int lq_adc_read(uint8_t channel, uint16_t *value) {
    if (!lq_hil_is_active()) {
        *value = 0;
        return -ENODEV;
    }
    
    struct lq_hil_adc_msg msg;
    
    /* Receive ADC injection from tester */
    if (lq_hil_sut_recv_adc(&msg, 100) != 0) {
        return -EAGAIN;
    }
    
    /* Verify channel matches */
    if (msg.hdr.channel != channel) {
        return -EINVAL;
    }
    
    *value = msg.value;
    return 0;
}
```

**CAN Send** - Forwards to tester for verification:
```c
int lq_can_send(uint32_t can_id, bool is_extended, 
                const uint8_t *data, uint8_t len) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    struct lq_hil_can_msg msg;
    msg.hdr.type = LQ_HIL_MSG_CAN;
    msg.hdr.timestamp_us = lq_platform_get_time_us();
    msg.can_id = can_id;
    msg.is_extended = is_extended;
    msg.dlc = len;
    memcpy(msg.data, data, len);
    
    return lq_hil_sut_send_can(&msg);
}
```

**GPIO Set** - Notifies tester of output changes:
```c
int lq_gpio_set(uint8_t pin, bool value) {
    if (!lq_hil_is_active()) {
        return -ENODEV;
    }
    
    return lq_hil_sut_send_gpio(pin, value ? 1 : 0);
}
```

## Building with HIL Platform

### For Testing (software only):

```bash
cd build
make hil-sut  # Builds with lq_platform_hil.c
make hil-quick  # Build + run tests
```

### For Production (real hardware):

```bash
cd build
cmake -DLQ_PLATFORM=zephyr ..  # Uses lq_platform_zephyr.c
make
```

## Integration with Generated Code

When you generate code with `dts_gen.py`, the generated `lq_generated.c` includes:

```c
// Generated initialization
int lq_generated_init(void) {
    // Setup queues, voters, merges...
    // Initialize hardware sources (ADC, SPI, CAN)
    // All hardware calls go through lq_platform.h
    return 0;
}
```

To run this in HIL mode:

1. **Build with HIL platform**:
   ```bash
   gcc real_sut.c lq_generated.c ... lq_platform_hil.c -o sut
   ```

2. **Run in HIL mode**:
   ```bash
   LQ_HIL_MODE=sut ./sut
   ```

3. **Run tester**:
   ```bash
   LQ_HIL_MODE=tester ./test_runner
   ```

The application runs normally, but all hardware I/O is intercepted!

## Benefits

### ✅ **Zero Code Changes**
- Same application runs in HIL and production
- No `#ifdef TEST` needed
- Generated code never knows it's being tested

### ✅ **Real Application Logic**
- Tests actual voting, merging, latency
- Validates timeout handling
- Verifies signal processing

### ✅ **Hardware Independence**
- Develop on Linux laptop
- Test without hardware
- CI/CD on any platform

### ✅ **Deterministic Testing**
- Control all inputs precisely
- Measure exact timing
- Reproducible results

## Example: RPM Voting System

**Generated Code**:
```c
void lq_adc_isr_rpm_adc(uint16_t value) {
    // Generated by dts_gen.py from:
    // rpm_adc: hw-source@0 { channel = <0>; }
    lq_hw_push(0, value);  // Pushes to queue 0
}
```

**Native Mode**: ISR called by real ADC hardware  
**HIL Mode**: "ISR" triggered by tester injection

**HIL Test**:
```dts
hil-test-rpm-voting {
    inject-adc = <0 2000>;  /* ADC0 = 2000 RPM */
    inject-adc = <1 2010>;  /* ADC1 = 2010 RPM */
    inject-adc = <2 1990>;  /* ADC2 = 1990 RPM */
    
    expect-can {
        pgn = <0xF004>;     /* EEC1 */
        spn = <190>;        /* Engine Speed */
        value = <2000>;     /* Voted result */
        tolerance = <10>;   /* ±10 RPM */
    };
};
```

**What happens**:
1. Tester sends 3 ADC values via HIL messages
2. SUT receives via `lq_adc_read()` → thinks it's real ADC
3. Values flow through voting logic
4. SUT sends result via `lq_can_send()` → goes to tester
5. Tester verifies voted output matches expected

**Same code, tested without hardware!**

## Current Status

### ✅ Implemented
- HIL platform layer (lq_platform_hil.c)
- ADC read interception
- CAN send interception
- GPIO set interception
- Automatic mode detection
- Build system integration

### 🚧 Needs Integration
- Update code generator to use new lq_hw_push() API
- Regenerate sample applications
- Add GPIO input injection to test format

### 📋 Next Steps
1. Update `dts_gen.py` to match current API
2. Regenerate `lq_generated.c` for samples
3. Build real SUT with full generated code
4. Add more test scenarios

## See Also

- [HIL Testing Guide](HIL_TESTING.md) - Overall HIL architecture
- [CI/CD Setup](CI_HIL_SETUP.md) - Hardware testing in CI
- [Platform API](../include/lq_platform.h) - Platform abstraction layer
