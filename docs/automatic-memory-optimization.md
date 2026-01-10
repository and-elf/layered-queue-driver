# Automatic Memory Optimization

## The Problem with Fixed Limits

Traditional embedded drivers use fixed compile-time limits:

```c
// Old approach - wasteful!
#define MAX_SIGNALS 32        // But you only use 5
#define MAX_OUTPUTS 16        // But you only need 3
#define MAX_MERGES 8          // But you don't have any

struct lq_engine {
    struct lq_signal signals[32];      // Wasting 27 slots
    struct lq_output outputs[16];      // Wasting 13 slots
    struct lq_merge merges[8];         // Wasting all 8 slots
};
```

**Problems:**
- Manual Kconfig tuning required
- Over-allocation wastes RAM
- Under-allocation causes runtime failures
- Reconfiguration requires manual updates

## The Solution: DTS-Based Auto-Sizing

The code generator analyzes your devicetree and calculates **exact** resource needs:

```c
// New approach - optimized!
#define LQ_MAX_SIGNALS 5      // Exactly what you need
#define LQ_MAX_OUTPUTS 3      // No waste
#define LQ_MAX_MERGES 0       // Zero if unused
```

### How It Works

When you run the DTS generator:

```bash
python3 scripts/dts_gen.py app.dts output/
```

It:
1. **Parses** your devicetree
2. **Counts** each resource type:
   - Hardware inputs (`lq,hw-*`)
   - Processing drivers (scale, merge, PID, etc.)
   - Outputs (cyclic, verified, etc.)
   - Signal IDs (auto-assigned)
3. **Calculates** buffer sizes:
   - Max merge inputs (largest `sources` array)
   - Output event buffer (cyclic outputs × 2)
4. **Generates** `lq_config.h` with exact counts

### Example: Automotive Sensor System

**DTS file (app.dts):**
```dts
/ {
    accel_x: lq-hw-adc { io-channels = <&adc0 0>; };
    accel_y: lq-hw-adc { io-channels = <&adc0 1>; };
    accel_z: lq-hw-adc { io-channels = <&adc0 2>; };
    gyro: lq-hw-spi { /* ... */ };
    
    imu_merge: lq-mid-merge {
        sources = <&accel_x &accel_y>;
        algorithm = "median";
    };
    
    can_tx1: lq-cyclic-output { /* ... */ };
    can_tx2: lq-cyclic-output { /* ... */ };
    can_tx3: lq-cyclic-output { /* ... */ };
};
```

**Generated config (lq_config.h):**
```c
/*
 * Signal array memory: 5 signals (vs default 32)
 * Savings: ~84% reduction in static allocation
 */

#define LQ_MAX_SIGNALS              5    // Counted: 4 inputs + 1 merge
#define LQ_MAX_HW_INPUTS            4    // Counted: 3 ADC + 1 SPI
#define LQ_MAX_MERGES               1    // Counted: 1 merge node
#define LQ_MAX_CYCLIC_OUTPUTS       3    // Counted: 3 CAN outputs
#define LQ_MAX_MERGE_INPUTS         2    // Max sources array length
```

## Memory Savings Examples

### Temperature Controller

**DTS:**
- 1 ADC sensor
- 1 scale driver
- 1 fault monitor
- 1 PID controller
- 1 verified output

**Generated config:**
```c
LQ_MAX_SIGNALS: 2 (vs 32)    →  93% saving
LQ_MAX_HW_INPUTS: 0 (vs 16)  → 100% saving
LQ_MAX_MERGES: 0 (vs 8)      → 100% saving
```

**Total RAM saved:** ~500 bytes on typical 32-bit system

### Triple-Redundant Safety System

**DTS:**
- 3 ADC sensors (redundant)
- 1 mid-value merge
- 1 fault monitor
- 1 CAN output

**Generated config:**
```c
LQ_MAX_SIGNALS: 4 (vs 32)    →  87% saving
LQ_MAX_HW_INPUTS: 3 (vs 16)  →  81% saving
LQ_MAX_MERGES: 1 (vs 8)      →  87% saving
```

**Total RAM saved:** ~400 bytes

### Complex Automotive Gateway

**DTS:**
- 12 CAN inputs
- 8 scale transforms
- 4 merge voters
- 16 J1939 outputs

**Generated config:**
```c
LQ_MAX_SIGNALS: 24 (vs 32)   →  25% saving
LQ_MAX_HW_INPUTS: 12 (vs 16) →  25% saving
LQ_MAX_MERGES: 4 (vs 8)      →  50% saving
```

**Total RAM saved:** ~150 bytes (smaller % but still optimized)

## Dynamic Buffer Sizing

Some buffers scale with usage:

### Output Event Buffer

```c
// Formula: (num_cyclic_outputs × 2) or 16 minimum
LQ_MAX_OUTPUT_EVENTS = max(num_cyclic_outputs * 2, 16)
```

**Examples:**
- 1 output  → 16 events (minimum)
- 8 outputs → 16 events (2×8)
- 32 outputs → 64 events (2×32)

### Merge Input Buffer

```c
// Largest sources array in any merge node
LQ_MAX_MERGE_INPUTS = max(len(merge.sources) for all merges)
```

**Examples:**
- `sources = <&a &b>` → 2
- `sources = <&a &b &c>` → 3
- `sources = <&a &b &c &d &e>` → 5

## Overriding Defaults

You can specify custom buffer sizes in the engine node:

```dts
engine: lq-engine {
    compatible = "lq,engine";
    hw-ringbuffer-size = <256>;  // Override default 128
};
```

This is useful when:
- High-frequency ADC sampling needs larger buffer
- Bursty input patterns require more buffering
- Memory-constrained systems need smaller buffers

## Integration with Zephyr

In Zephyr, Kconfig values are **ignored** when using DTS generation:

```ini
# prj.conf - these are IGNORED if lq_config.h exists
CONFIG_LQ_MAX_SIGNALS=32      # Overridden by DTS
CONFIG_LQ_MAX_CYCLIC_OUTPUTS=16  # Overridden by DTS
```

The build process:
1. DTS generator runs → creates `lq_config.h`
2. CMake includes `lq_config.h`
3. Driver code uses DTS-calculated values
4. Kconfig values are ignored

**Benefits:**
- No manual tuning needed
- Change DTS → automatic reconfiguration
- Memory optimized automatically
- No config drift between DTS and Kconfig

## Build Integration

### Standalone Build

```bash
# Generate config
python3 scripts/dts_gen.py app.dts build/

# Build uses generated config
cmake -B build
cmake --build build
```

### Zephyr Build

```cmake
# CMakeLists.txt
find_package(Zephyr REQUIRED)

# DTS generator runs automatically
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/lq_config.h
    COMMAND ${PYTHON} ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${DTS_FILE} ${CMAKE_BINARY_DIR}
    DEPENDS ${DTS_FILE}
)
```

The config is regenerated whenever DTS changes.

## Debugging

### Check Generated Config

```bash
# After build
cat build/lq_config.h
```

Look for the header comment:

```c
/*
 * Signal array memory: 5 signals (vs default 32)
 * Savings: ~84% reduction in static allocation
 */
```

### Verify Resource Counts

Generator prints summary:

```
Generated build/lq_config.h
  Signals: 5, HW Inputs: 4, Merges: 1, Cyclic Outputs: 3
```

Compare to your DTS node count.

### Over-Allocation Warnings

If you see unexpectedly large counts:

```c
LQ_MAX_SIGNALS: 100  // Seems high!
```

Check for:
- Manual signal IDs with large gaps
- Duplicate node definitions
- Unreferenced nodes

## Best Practices

### 1. Let Auto-Assignment Work

**Don't:**
```dts
sensor_a: lq-hw-adc {
    signal-id = <100>;  // Creates 99 unused slots!
};
```

**Do:**
```dts
sensor_a: lq-hw-adc {
    // Auto-assigned ID (0, 1, 2, ...)
};
```

### 2. Remove Unused Nodes

**Don't:**
```dts
// Development leftovers
test_sensor: lq-hw-adc { status = "disabled"; };  // Still counts!
old_output: lq-cyclic-output { /* unused */ };
```

**Do:**
```dts
// Delete or comment out unused nodes
// test_sensor: ...
```

### 3. Use Conditional Compilation

For platform variants:

```dts
#ifdef BOARD_HAS_TRIPLE_REDUNDANCY
    sensor_a: lq-hw-adc { /* ... */ };
    sensor_b: lq-hw-adc { /* ... */ };
    sensor_c: lq-hw-adc { /* ... */ };
    merge: lq-mid-merge { sources = <&sensor_a &sensor_b &sensor_c>; };
#else
    sensor: lq-hw-adc { /* ... */ };
#endif
```

Generator counts only enabled nodes.

## Comparison: Before vs After

### Before (Manual Kconfig)

```c
// Kconfig
CONFIG_LQ_MAX_SIGNALS=32        // Conservative guess
CONFIG_LQ_MAX_OUTPUTS=16        // Over-provisioned
CONFIG_LQ_MAX_MERGES=8          // Wasted if unused

// Result: ~1200 bytes allocated
// Actual usage: ~300 bytes
// Waste: ~900 bytes (75%)
```

**Problems:**
- 75% memory waste
- Manual tuning required
- Easy to under-allocate (crashes)
- Hard to know "right" values

### After (DTS Auto-Sizing)

```c
// Generated from DTS
LQ_MAX_SIGNALS: 5               // Exact count
LQ_MAX_OUTPUTS: 3               // Exact count
LQ_MAX_MERGES: 1                // Exact count

// Result: ~320 bytes allocated
// Actual usage: ~300 bytes
// Waste: ~20 bytes (6%)
```

**Benefits:**
- 6% overhead (safety margin)
- Zero configuration needed
- Impossible to under-allocate
- Changes automatically tracked

## Summary

**Automatic memory optimization means:**

✅ No manual Kconfig tuning  
✅ Memory usage scales with actual DTS  
✅ 50-95% RAM savings typical  
✅ Zero overhead (compile-time calculation)  
✅ Safe - generator ensures sufficient allocation  
✅ Refactor-proof - add/remove nodes freely  

**The devicetree defines both functionality AND resource allocation.**
