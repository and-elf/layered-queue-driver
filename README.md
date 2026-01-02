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

```
┌─────────────┐
│   Hardware  │
│  (ADC/SPI/  │
│   GPIO)     │
└──────┬──────┘
       │
┌──────▼──────────┐
│  Source Driver  │ ◄── Range/Value Validation
│  (lq-adc-source,│     Error Detection
│   lq-spi-source)│
└──────┬──────────┘
       │
┌──────▼──────┐
│    Queue    │ ◄── Buffering, Drop Policy
│  (lq-queue) │     Statistics
└──────┬──────┘
       │
┌──────▼──────────┐
│ Merge/Voter     │ ◄── Redundancy, Voting
│ (lq-merge-voter)│     Tolerance Checking
└──────┬──────────┘
       │
┌──────▼──────┐
│    Queue    │
└──────┬──────┘
       │
┌──────▼──────────┐
│  Application    │
└─────────────────┘
```

## Quick Start

### 1. Device Tree Configuration

Define queues, sources, and merge logic in your device tree:

```dts
/ {
    /* Define queues */
    q_pressure: lq-queue@0 {
        compatible = "zephyr,lq-queue";
        capacity = <16>;
        drop-policy = "drop-oldest";
    };

    /* ADC source with range validation */
    adc_pressure: lq-source@0 {
        compatible = "zephyr,lq-adc-source";
        adc = <&adc1>;
        channel = <2>;
        output-queue = <&q_pressure>;
        poll-interval-ms = <100>;

        valid {
            min = <1000>;
            max = <3000>;
            status = <0>;  /* LQ_OK */
        };

        degraded {
            min = <900>;
            max = <3100>;
            status = <1>;  /* LQ_DEGRADED */
        };
    };

    /* Merge redundant inputs */
    merged: lq-merge@0 {
        compatible = "zephyr,lq-merge-voter";
        input-queues = <&q_adc1 &q_adc2>;
        output-queue = <&q_merged>;
        voting-method = "median";
        tolerance = <50>;
    };
};
```

### 2. Application Code

Use the queue API to consume data:

```c
#include <zephyr/drivers/layered_queue.h>

const struct device *queue = DEVICE_DT_GET(DT_NODELABEL(q_pressure));

void monitor_pressure(void)
{
    struct lq_item item;
    
    while (1) {
        if (lq_pop(queue, &item, K_FOREVER) == 0) {
            printk("Pressure: %d (status=%d)\n", 
                   item.value, item.status);
            
            if (item.status != LQ_OK) {
                handle_fault();
            }
        }
    }
}
```

### 3. Build and Run

```bash
west build -b <your_board> samples/basic
west flash
```

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
zephyr-declarative-drivers/
├── dts/
│   ├── bindings/layered-queue/     # Device tree bindings
│   │   ├── zephyr,lq-queue.yaml
│   │   ├── zephyr,lq-adc-source.yaml
│   │   ├── zephyr,lq-spi-source.yaml
│   │   ├── zephyr,lq-merge-voter.yaml
│   │   └── zephyr,lq-dual-inverted.yaml
│   └── examples/                   # Example device trees
│       ├── layered-queue-example.dts
│       └── automotive-brake-example.dts
├── include/zephyr/drivers/
│   ├── layered_queue.h             # Public API
│   └── layered_queue_internal.h    # Internal structures
├── drivers/layered_queue/          # Driver implementations
│   ├── lq_queue.c
│   ├── lq_adc_source.c
│   ├── lq_spi_source.c
│   ├── lq_merge_voter.c
│   ├── lq_util.c
│   ├── Kconfig
│   └── CMakeLists.txt
├── samples/basic/                  # Example application
│   ├── src/main.c
│   └── README.md
├── tests/util/                     # Unit tests
│   └── src/main.c
└── docs/
    └── architecture.md             # Detailed documentation
```

## Examples

### Example 1: Basic ADC Monitoring

[See dts/examples/layered-queue-example.dts](dts/examples/layered-queue-example.dts)

### Example 2: Automotive Brake System

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
