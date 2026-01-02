# Layered Queue Driver for Zephyr

This driver can be used both as:
1. **Standalone library** - Build and test independently with CMake
2. **Zephyr module** - Integrate into Zephyr projects via west

## Standalone Build (Native Testing)

```bash
# Build
cmake -B build
cmake --build build

# Test
cd build && ctest --output-on-failure

# Coverage
cmake -B build -DENABLE_COVERAGE=ON
cmake --build build
cd build && ctest
```

## Zephyr Integration

### As a Zephyr Module

Add to your Zephyr project's `west.yml`:

```yaml
manifest:
  projects:
    - name: layered-queue
      url: https://github.com/your-org/zephyr-declarative-drivers
      revision: main
      path: modules/layered-queue
```

Then in your `prj.conf`:

```ini
CONFIG_LQ_DRIVER=y
CONFIG_LQ_QUEUE=y
CONFIG_LQ_ADC_SOURCE=y
CONFIG_LQ_SPI_SOURCE=y
CONFIG_LQ_MERGE_VOTER=y
```

### Device Tree Example

```dts
/ {
    q_pressure: lq-queue@0 {
        compatible = "zephyr,lq-queue";
        capacity = <16>;
        drop-policy = "drop-oldest";
    };

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
    };
};
```

### Application Code

```c
#include <zephyr/drivers/layered_queue.h>

const struct device *queue = DEVICE_DT_GET(DT_NODELABEL(q_pressure));

void main(void) {
    struct lq_item item;
    
    while (1) {
        if (lq_pop(queue, &item, K_FOREVER) == 0) {
            printk("Value: %d, Status: %d\n", item.value, item.status);
        }
    }
}
```

## Architecture

```
┌─────────────────────────────────────┐
│   Application Code                   │
│   (Native or Zephyr)                │
└────────────┬────────────────────────┘
             │
┌────────────▼────────────────────────┐
│   Platform-Independent Core         │
│   - lq_queue_core.c                 │
│   - layered_queue_core.h            │
└────────────┬────────────────────────┘
             │
     ┌───────┴────────┐
     │                │
┌────▼─────┐   ┌─────▼────┐
│  Native  │   │  Zephyr  │
│ Platform │   │ Platform │
│ (POSIX)  │   │ (Kernel) │
└──────────┘   └──────────┘
```

## Directory Structure

```
.
├── CMakeLists.txt           # Standalone CMake build
├── include/
│   ├── layered_queue_core.h # Platform-independent API
│   ├── lq_platform.h        # Platform abstraction
│   └── zephyr/drivers/
│       ├── layered_queue.h  # Zephyr device API
│       └── layered_queue_internal.h
├── src/
│   ├── lq_queue_core.c      # Core implementation
│   └── platform/
│       ├── lq_platform_native.c   # POSIX impl
│       └── lq_platform_zephyr.c   # Zephyr impl
├── drivers/                 # Zephyr module
│   ├── CMakeLists.txt
│   └── layered_queue/
│       ├── Kconfig
│       ├── lq_queue.c       # DT wrapper
│       └── lq_*.c           # Source drivers
├── tests/
│   └── queue_test.cpp       # Google Test suite
├── dts/
│   ├── bindings/            # DT bindings
│   └── examples/            # Example overlays
└── zephyr/
    └── module.yml           # Zephyr module definition
```

## Features

- ✅ Platform-independent core
- ✅ Full test coverage with GoogleTest
- ✅ Zephyr device tree integration
- ✅ CI/CD ready (native + Zephyr builds)
- ✅ No dynamic allocation
- ✅ Thread-safe operations
- ✅ Configurable drop policies
- ✅ Statistics tracking

## License

Apache-2.0
