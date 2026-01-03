# Layered Queue Driver for Zephyr RTOS

A declarative, device-tree-driven framework for building robust data pipelines in safety-critical embedded systems. The Layered Queue Driver enables hardware sensor abstraction, range validation, redundancy management, and fault detection—all configured through device tree.

## Features

- **Declarative Configuration**: Define entire data pipelines in device tree
- **Hardware Abstraction**: ADC, SPI, GPIO sources with unified queue interface
- **Range Validation**: Per-source value range checking with status classification
- **Redundancy Management**: Merge/vote on multiple redundant inputs with configurable algorithms
- **Fault Detection**: Dual-inverted inputs, tolerance checking, timeout detection
- **Safety-Critical Ready**: Thread-safe queues, deterministic behavior, error propagation
- **Statistics**: Built-in monitoring for drops, peak usage, throughput

## Architecture

The Layered Queue Driver uses a clean layered architecture separating hardware concerns from pure processing logic:

```
┌─────────────────────────────────────────────┐
│         Hardware ISR / Polling              │ ← RTOS-aware, minimal
│  (ADC callbacks, SPI reads, GPIO events)    │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_push()
┌─────────────────▼───────────────────────────┐
│      Layer 2: Input Aggregator              │ ← RTOS-aware, thin
│         (Hardware ringbuffer)               │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_pop()
┌─────────────────▼───────────────────────────┐
│    Mid-level Drivers (vtable pattern)       │ ← PURE (no RTOS)
│  • ADC validator                            │
│  • SPI validator                            │
│  • Merge/Voter                              │
│  • Range checker                            │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│         ENGINE STEP (pure, once)            │ ← Deterministic
│    Process all inputs → Generate events     │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│          Output Drivers                     │ ← Hardware adapters
│  • CAN  • GPIO  • UART                      │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│       Sync Groups (atomic updates)          │
└─────────────────────────────────────────────┘
```

**Key Benefits:**
- **Testability**: Pure mid-level drivers and engine step are easily unit tested
- **Portability**: Hardware abstraction enables running on any platform
- **Determinism**: Single engine step processes all inputs predictably
- **Safety**: Clear separation enables formal verification of pure layers

## Quick Start

### 1. Hardware Layer - Capture Raw Samples

Connect hardware interrupts to the input aggregator:

```c
#include "lq_hw_input.h"

// ADC interrupt callback
void adc_isr_callback(uint16_t sample)
{
    lq_hw_push(LQ_HW_ADC0, sample);  // ISR-safe
}

// SPI polling task
void spi_poll_task(void)
{
    while (1) {
        uint32_t value = spi_read_sensor();
        lq_hw_push(LQ_HW_SPI0, value);
        k_sleep(K_MSEC(10));
    }
}
```

### 2. Device Tree Configuration

Define the complete pipeline in device tree:

```dts
/ {
    /* Hardware inputs */
    inputs {
        adc0: lq-adc@0 {
            compatible = "lq,adc";
            channel = <0>;
            min-raw = <0>;
            max-raw = <4095>;
            stale-us = <5000>;
        };
        
        spi0: lq-spi@0 {
            compatible = "lq,spi";
            channel = <0>;
            stale-us = <5000>;
        };
    };
    
    /* Mid-level processing */
    merges {
        engine_sensor: lq-merge@0 {
            inputs = <&adc0 &spi0>;
            tolerance = <50>;
            voting-method = "median";
        };
    };
    
    /* Output adapters */
    outputs {
        can_speed: lq-output@0 {
            compatible = "lq,can-output";
            source = <&engine_sensor>;
            pgn = <0xFEF1>;
            period-us = <100000>;  /* 10Hz */
        };
        
        gpio_warning: lq-output@1 {
            compatible = "lq,gpio-output";
            source = <&engine_sensor>;
            pin = <5>;
            trigger-status = <1>;  /* >= DEGRADED */
        };
    };
    
    /* Synchronized updates */
    sync_groups {
        snapshot: lq-sync-group@0 {
            period-us = <100000>;
            members = <&can_speed &gpio_warning>;
        };
    };
};
```

### 3. Engine Processing

The engine step runs periodically to process all pending samples:

```c
#include "lq_sync_group.h"
#include "lq_hw_input.h"
#include "lq_mid_driver.h"
#include "lq_platform.h"

void engine_task(void)
{
    struct lq_engine *engine = get_engine_instance();
    struct lq_event events[MAX_EVENTS];
    
    while (1) {
        uint64_t now = lq_platform_uptime_get() * 1000; // Convert to μs
        size_t num_events = 0;
        
        // Collect events from all pending hardware samples
        struct lq_hw_sample sample;
        while (lq_hw_pop(&sample) == 0 && num_events < MAX_EVENTS) {
            struct lq_mid_driver *drv = find_driver_for_source(sample.src);
            num_events += lq_mid_process(drv, now, &sample,
                                        &events[num_events],
                                        MAX_EVENTS - num_events);
        }
        
        // Single deterministic processing step
        lq_engine_step(engine, now, events, num_events);
        
        // Transmit all output events
        for (size_t i = 0; i < engine->out_event_count; i++) {
            transmit_output_event(&engine->out_events[i]);
        }
        
        k_sleep(K_USEC(100));  // 10kHz processing rate
    }
}
```

The engine step internally:
1. Ingests events into canonical signals
2. Applies staleness detection
3. Processes merge/voter logic
4. Generates on-change outputs
5. Processes cyclic deadline-scheduled outputs

This replaces the old queue-based approach with a cleaner, more testable design.

## Driver Types

### Queue (`zephyr,lq-queue`)

Buffered FIFO queue for data items with configurable capacity and drop policies.

**Properties:**
- `capacity`: Maximum items
- `drop-policy`: `drop-oldest`, `drop-newest`, or `block`
- `priority`: Scheduling priority

### ADC Source (`zephyr,lq-adc-source`)

Polls ADC channels and validates against expected ranges.

**Properties:**
- `adc`: ADC device reference
- `channel`: ADC channel number
- `output-queue`: Destination queue
- `poll-interval-ms`: Polling rate
- `averaging`: Number of samples to average
- Child nodes: Range definitions with `min`, `max`, `status`

### SPI Source (`zephyr,lq-spi-source`)

Reads SPI devices and validates against expected discrete values.

**Properties:**
- `spi`: SPI bus reference
- `reg`: Device register/CS
- `output-queue`: Destination queue
- `poll-interval-ms`: Polling rate
- Child nodes: Expected values with `value`, `status`

### Merge/Voter (`zephyr,lq-merge-voter`)

Combines multiple redundant inputs using configurable voting algorithms.

**Properties:**
- `input-queues`: Array of input queue references
- `output-queue`: Destination queue
- `voting-method`: `median`, `average`, `min`, `max`, `majority`
- `tolerance`: Maximum allowed deviation between inputs
- `timeout-ms`: Timeout for stale data
- Child nodes: Output range validation

### Dual-Inverted (`zephyr,lq-dual-inverted`)

Monitors complementary GPIO signals for fault detection.

**Properties:**
- `gpio-normal`: Normal signal GPIO
- `gpio-inverted`: Inverted signal GPIO
- `output-queue`: Destination queue
- `debounce-ms`: Debounce time
- `error-on-both-high`: Error when both signals high
- `error-on-both-low`: Error when both signals low

## Status Codes

```c
enum lq_status {
    LQ_OK = 0,              // Normal operation
    LQ_DEGRADED = 1,        // Degraded but functional
    LQ_OUT_OF_RANGE = 2,    // Out of acceptable range
    LQ_ERROR = 3,           // Hardware/communication error
    LQ_TIMEOUT = 4,         // Data timeout
    LQ_INCONSISTENT = 5,    // Redundant sources disagree
};
```

## Directory Structure

```
layered-queue-driver/
├── dts/
│   ├── bindings/layered-queue/     # Device tree bindings
│   │   ├── lq,adc.yaml
│   │   ├── lq,spi.yaml
│   │   ├── lq,merge-voter.yaml
│   │   ├── lq,can-output.yaml
│   │   ├── lq,gpio-output.yaml
│   │   └── lq,sync-group.yaml
│   └── examples/                   # Example device trees
│       ├── layered-architecture-example.dts
│       ├── layered-queue-example.dts (legacy)
│       └── automotive-brake-example.dts (legacy)
├── include/
│   ├── lq_hw_input.h              # Hardware input layer (ISR-safe)
│   ├── lq_mid_driver.h            # Mid-level driver interface (pure)
│   ├── lq_event.h                 # Event and output driver system
│   ├── lq_sync_group.h            # Synchronized output groups
│   ├── lq_platform.h              # Platform abstraction
│   ├── lq_util.h                  # Utility functions
│   ├── layered_queue_core.h       # Legacy core API
│   └── zephyr/drivers/
│       └── layered_queue.h        # Legacy public API
├── src/
│   ├── lq_hw_input.c              # Hardware input implementation
│   ├── lq_engine.c                # Pure engine step
│   ├── lq_queue_core.c            # Legacy queue implementation
│   ├── lq_util.c                  # Utility functions
│   └── platform/
│       ├── lq_platform_native.c   # Native/POSIX platform
│       └── lq_platform_zephyr.c   # Zephyr RTOS platform
├── drivers/layered_queue/          # Zephyr driver implementations
│   ├── lq_adc_input.c             # ADC input driver
│   ├── lq_spi_input.c             # SPI input driver
│   ├── lq_merge_voter.c           # Merge/voter mid-level driver
│   ├── lq_can_output.c            # CAN output driver
│   ├── lq_gpio_output.c           # GPIO output driver
│   ├── lq_sync_group.c            # Sync group implementation
│   ├── Kconfig
│   └── CMakeLists.txt
├── samples/
│   ├── basic/                     # Legacy basic example
│   └── layered/                   # New layered architecture example
├── tests/
│   ├── queue_test.cpp             # Queue tests
│   ├── engine_test.cpp            # Engine step tests (pure)
│   └── util/                      # Utility tests
└── docs/
    ├── architecture.md            # Architecture documentation
    ├── devicetree-guide.md        # Device tree guide
    └── testing.md                 # Testing guide
```

## Examples

### Example 1: Layered Architecture - Engine Monitoring

Complete example showing the new layered design with inputs, mid-level processing, outputs, and sync groups:

[See dts/examples/layered-architecture-example.dts](dts/examples/layered-architecture-example.dts)

### Example 2: Basic ADC Monitoring (Legacy)

[See dts/examples/layered-queue-example.dts](dts/examples/layered-queue-example.dts)

### Example 3: Automotive Brake System (Legacy)

Dual redundant ADC sensors with median voting and strict tolerance:

[See dts/examples/automotive-brake-example.dts](dts/examples/automotive-brake-example.dts)

### Example 3: Safety-Critical Merged Inputs

```dts
merged_critical: lq-merge@0 {
    compatible = "zephyr,lq-merge-voter";
    input-queues = <&q_adc &q_spi>;
    output-queue = <&q_merged>;
    tolerance = <50>;
    voting-method = "median";
    
    expected-range {
        min = <1100>;
        max = <2900>;
        status-if-violation = <2>;  /* LQ_OUT_OF_RANGE */
    };
};
```

## Configuration

Enable in `prj.conf`:

```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_QUEUE=y
CONFIG_LQ_ADC_SOURCE=y
CONFIG_LQ_SPI_SOURCE=y
CONFIG_LQ_MERGE_VOTER=y
CONFIG_LQ_DUAL_INVERTED=y
CONFIG_LQ_LOG_LEVEL_INF=y
```

## Building and Testing

The core queue system is platform-independent and can be built and tested standalone without Zephyr:

### Build

```bash
cmake -B build
cmake --build build
```

### Run Tests

```bash
cd build
ctest --output-on-failure
```

Or use the convenience target:

```bash
cmake --build build --target check
```

### With Coverage

```bash
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
cd build && ctest --output-on-failure
lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch
genhtml coverage.info --output-directory coverage_html
```

## Use Cases

- **Automotive**: Brake pressure monitoring, throttle position sensing
- **Industrial**: Temperature control, motor current monitoring
- **Aerospace**: Attitude sensors, redundant IMUs
- **Medical**: Vital sign monitoring with fault detection
- **Robotics**: Multi-sensor fusion with outlier rejection

## Performance

- Queue operations: O(1) push/pop with mutex protection
- Voting algorithms: O(n log n) for median, O(n) for average/min/max
- Memory: Static allocation, no runtime malloc
- Polling overhead: Configurable per source (typical 10-1000ms)

## Safety Features

- Thread-safe queue operations
- Deterministic behavior (no dynamic allocation)
- Error propagation through status codes
- Range validation at source level
- Timeout detection for stale data
- Consistency checking for redundant inputs
- Statistics for monitoring and diagnostics

## Future Enhancements

- [ ] CAN bus source driver
- [ ] I2C source driver
- [ ] Filter nodes (moving average, Kalman)
- [ ] Timestamp synchronization for multi-source merge
- [ ] DMA support for high-speed ADC
- [ ] Runtime configuration API
- [ ] Diagnostic shell commands

## License

Apache-2.0

## Contributing

Contributions welcome! Please ensure:
- Device tree bindings follow Zephyr conventions
- Code passes unit tests
- Documentation updated for new features
- Examples provided for new driver types
