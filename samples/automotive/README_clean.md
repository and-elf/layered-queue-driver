# Automotive Engine Monitor Sample

This sample demonstrates **pure C code** with **build-time code generation** from devicetree.

## Clean Architecture

**No macros in application code. No RTOS dependencies (optional).**

### Build Process

```
app.dts → scripts/dts_gen.py → lq_generated.c/h → Compile → Binary
```

1. **DTS Configuration** (`app.dts`): Declarative system definition
2. **Code Generator** (`scripts/dts_gen.py`): Parses DTS, generates C code
3. **Generated Code** (`lq_generated.c/h`): Complete engine struct + ISR handlers
4. **Application** (`main.c`): Clean C code, just includes and uses generated code

### Application Code

```c
#include "lq_engine.h"
#include "lq_generated.h"
#include "lq_log.h"

int main(void) {
    lq_generated_init();          /* Hardware setup */
    lq_engine_start(&g_lq_engine);  /* Start processing */
    return 0;
}
```

**That's it!** No macros, no devicetree manipulation in application code.

## What's Generated

From `app.dts`, the generator creates:

### lq_generated.h
```c
extern struct lq_engine g_lq_engine;
int lq_generated_init(void);
```

### lq_generated.c
```c
/* Merge contexts */
static struct lq_merge_ctx g_merges[1] = {
    [0] = {
        .output_signal = 10,
        .input_signals = {0, 1},
        .voting_method = LQ_VOTE_MEDIAN,
        .tolerance = 50,
        ...
    },
};

/* Cyclic outputs */
static struct lq_cyclic_ctx g_cyclic_outputs[3] = { ... };

/* Engine instance */
struct lq_engine g_lq_engine = {
    .num_signals = 32,
    .num_merges = 1,
    .num_cyclic_outputs = 3,
    .merges = g_merges,
    .cyclic_outputs = g_cyclic_outputs,
};

/* ISR handlers */
void lq_adc_isr_rpm_adc(uint16_t value) {
    lq_hw_push(0, value, lq_platform_get_time_us());
}

void lq_spi_isr_rpm_spi(int32_t value) {
    lq_hw_push(1, value, lq_platform_get_time_us());
}

int lq_generated_init(void) {
    lq_hw_input_init();
    /* TODO: Configure ADC/SPI/Sensor triggers */
    return 0;
}
```

**All structs initialized at compile-time. No runtime overhead.**

## Platform Abstraction

### Logging

The logging API is platform-agnostic via `lq_log.h`:

```c
#include "lq_log.h"

LQ_LOG_MODULE_REGISTER(app, LQ_LOG_LEVEL_INF);
LQ_LOG_INF("RPM: %d", rpm);
LQ_LOG_ERR("Sensor failed");
```

**Backends:**
- Zephyr: Maps to Zephyr logging
- POSIX: Maps to printf
- Bare-metal: No-op (or custom implementation)

### RTOS Support

**Optional** - works with or without RTOS:

```c
#ifdef CONFIG_LQ_ENGINE_TASK
    /* Use RTOS task */
    lq_engine_start(&g_lq_engine);
#else
    /* Bare-metal loop */
    while (1) {
        lq_engine_step(&g_lq_engine);
        lq_platform_delay_ms(10);
    }
#endif
```

## Building

### Manual Generation
```bash
# Generate code from DTS
python3 scripts/dts_gen.py samples/automotive/app.dts samples/automotive/src/

# Build
west build -b <board> samples/automotive
```

### CMake Integration (Recommended)
Add to `CMakeLists.txt`:
```cmake
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_generated.c
           ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_generated.h
    COMMAND python3 ${CMAKE_SOURCE_DIR}/scripts/dts_gen.py
            ${CMAKE_CURRENT_SOURCE_DIR}/app.dts
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/app.dts
    COMMENT "Generating code from DTS"
)

add_custom_target(lq_codegen
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/lq_generated.c
)

target_sources(app PRIVATE
    src/main.c
    src/lq_generated.c
)

add_dependencies(app lq_codegen)
```

Then just:
```bash
west build -b <board> samples/automotive
west flash
```

Code is regenerated automatically when DTS changes!

## File Structure

```
samples/automotive/
├── app.dts                    # DTS configuration
├── src/
│   ├── main.c                 # Application (clean, no macros)
│   ├── lq_generated.h         # Generated header
│   └── lq_generated.c         # Generated implementation
├── CMakeLists.txt             # Build configuration
└── README.md                  # This file
```

## Benefits

✅ **Clean application code** - No macro soup  
✅ **Compile-time initialization** - Zero runtime overhead  
✅ **RTOS-free option** - Works on bare-metal  
✅ **Platform-agnostic logging** - Zephyr/POSIX/bare-metal  
✅ **Pure C** - No C++ or special compiler features  
✅ **Readable generated code** - Easy to debug  
✅ **Automatic regeneration** - CMake integration  

## Example DTS

```dts
/ {
    engine: lq-engine {
        compatible = "lq,engine";
        max-signals = <32>;
    };
    
    rpm_adc: lq-hw-adc-input@0 {
        compatible = "lq,hw-adc-input";
        signal-id = <0>;
        adc-channel = <0>;
        stale-us = <5000>;
    };
    
    rpm_merge: lq-mid-merge@0 {
        compatible = "lq,mid-merge";
        output-signal-id = <10>;
        input-signal-ids = <0 1>;
        voting-method = "median";
        tolerance = <50>;
    };
    
    can_rpm: lq-cyclic-output@0 {
        compatible = "lq,cyclic-output";
        source-signal-id = <10>;
        output-type = "j1939";
        target-id = <0xFEF1>;
        period-us = <100000>;
    };
};
```

**No code changes needed** - just modify DTS and rebuild!

## Comparison: Before vs After

### Before (Macro-based)
```c
#include <zephyr/kernel.h>
#include "lq_devicetree.h"

LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
```

### After (Generated)
```c
#include "lq_generated.h"

/* All code in lq_generated.c - clean and readable */
int main(void) {
    lq_generated_init();
    lq_engine_start(&g_lq_engine);
}
```

**Much cleaner!**
