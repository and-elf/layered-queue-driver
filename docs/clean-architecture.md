# Clean Architecture Summary

## Problem Solved

**Before:** Application code was filled with macro expansion:
```c
#include <zephyr/kernel.h>
#include "lq_devicetree.h"

LQ_FOREACH_HW_ADC_INPUT(LQ_GEN_ADC_ISR_HANDLER)
LQ_FOREACH_HW_SPI_INPUT(LQ_GEN_SPI_ISR_HANDLER)
static struct lq_engine engine = LQ_ENGINE_DT_INIT(DT_NODELABEL(engine));
```

**After:** Clean, readable C code:
```c
#include "lq_generated.h"
#include "lq_log.h"

int main(void) {
    lq_generated_init();
    lq_engine_start(&g_lq_engine);
}
```

## Architecture

### Build-Time Code Generation

```
┌─────────────┐
│   app.dts   │  Devicetree configuration
└──────┬──────┘
       │
       ▼
┌──────────────────┐
│ scripts/dts_gen.py│  Python code generator
└──────┬───────────┘
       │
       ▼
┌────────────────────┐
│  lq_generated.c/h  │  Pure C code
│                    │  - Compile-time initialized structs
│                    │  - ISR handlers
│                    │  - Init functions
└──────┬─────────────┘
       │
       ▼
┌────────────────────┐
│     main.c         │  Clean application
│  (no macros!)      │
└────────────────────┘
```

### Generated Code Example

**Input (DTS):**
```dts
rpm_merge: lq-mid-merge@0 {
    compatible = "lq,mid-merge";
    output-signal-id = <10>;
    input-signal-ids = <0 1>;
    voting-method = "median";
    tolerance = <50>;
};
```

**Output (C):**
```c
static struct lq_merge_ctx g_merges[1] = {
    [0] = {
        .output_signal = 10,
        .input_signals = {0, 1},
        .num_inputs = 2,
        .voting_method = LQ_VOTE_MEDIAN,
        .tolerance = 50,
        .stale_us = 10000,
    },
};

struct lq_engine g_lq_engine = {
    .num_signals = 32,
    .num_merges = 1,
    .merges = g_merges,
    ...
};
```

**All initialized at compile-time. Zero runtime overhead.**

## Platform Abstraction

### 1. Logging API

```c
#include "lq_log.h"

LQ_LOG_MODULE_REGISTER(app, LQ_LOG_LEVEL_INF);
LQ_LOG_INF("RPM: %d", rpm);
LQ_LOG_ERR("Sensor failed");
```

**Backends:**
- Zephyr → `LOG_INF()`, `LOG_ERR()`
- POSIX → `printf()`
- Bare-metal → no-op

### 2. RTOS Support

**Optional** - same code works with or without RTOS:

```c
#ifdef CONFIG_LQ_ENGINE_TASK
    lq_engine_start(&g_lq_engine);  /* RTOS task */
#else
    while (1) {
        lq_engine_step(&g_lq_engine);  /* Bare-metal loop */
        lq_platform_delay_ms(10);
    }
#endif
```

### 3. Time Abstraction

```c
uint64_t lq_platform_get_time_us(void);  /* Microsecond timestamp */
void lq_platform_delay_ms(uint32_t ms);  /* Platform delay */
```

- Zephyr: `k_uptime_get()`
- POSIX: `gettimeofday()`
- Bare-metal: Custom implementation

## File Structure

```
layered-queue-driver/
├── scripts/
│   └── dts_gen.py           # Code generator
├── include/
│   ├── lq_log.h              # Platform-agnostic logging
│   ├── lq_platform.h         # Platform abstraction
│   ├── lq_engine.h           # Engine API
│   └── ...
├── samples/automotive/
│   ├── app.dts               # System configuration
│   ├── src/
│   │   ├── main.c            # Application (clean!)
│   │   ├── lq_generated.h    # Generated header
│   │   └── lq_generated.c    # Generated implementation
│   └── README_clean.md       # Documentation
└── ...
```

## Benefits

✅ **Clean application code** - No macro expansion  
✅ **Compile-time initialization** - Zero runtime overhead  
✅ **RTOS-free option** - Works on bare-metal  
✅ **Platform-agnostic** - Zephyr, POSIX, bare-metal  
✅ **Readable generated code** - Easy to debug  
✅ **Pure C** - No C++ or special compiler features  
✅ **CMake integration** - Auto-regenerates on DTS changes  

## CMake Integration

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

add_dependencies(app lq_codegen)
```

**Code is regenerated automatically when DTS changes!**

## Usage

### Development
```bash
# Generate code from DTS
python3 scripts/dts_gen.py samples/automotive/app.dts samples/automotive/src/

# Build
west build -b <board> samples/automotive
west flash
```

### Production
Just build - CMake handles code generation automatically:
```bash
west build -b <board> samples/automotive
west flash
```

## Comparison

| Aspect | Before (Macros) | After (Generated) |
|--------|-----------------|-------------------|
| Application code | Macro soup | Pure C |
| Struct init | Runtime | Compile-time |
| RTOS dependency | Required | Optional |
| Readability | Poor | Excellent |
| Debug | Difficult | Easy |
| Performance | Good | Excellent |

## Example Application (Complete)

```c
#include "lq_generated.h"
#include "lq_log.h"

LQ_LOG_MODULE_REGISTER(app, LQ_LOG_LEVEL_INF);

int main(void) {
    LQ_LOG_INF("Starting engine monitor");
    
    int ret = lq_generated_init();
    if (ret != 0) {
        LQ_LOG_ERR("Init failed: %d", ret);
        return ret;
    }
    
#ifdef CONFIG_LQ_ENGINE_TASK
    lq_engine_start(&g_lq_engine);
#else
    while (1) {
        lq_engine_step(&g_lq_engine);
        lq_platform_delay_ms(10);
    }
#endif
    
    return 0;
}
```

**That's the entire application!** Clean, readable, platform-agnostic.
