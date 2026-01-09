# Arduino Library Build Notes

## Architecture

The Arduino library uses **relative includes** to access parent repository code without symlinks:

```
arduino/
├── src/
│   ├── LayeredQueue.h              # Main header (includes all functionality)
│   ├── LayeredQueue_Drivers.cpp    # Includes all .c files from parent
│   ├── LayeredQueue_BLDC.h         # BLDC C++ wrapper header
│   ├── LayeredQueue_BLDC.cpp       # BLDC C++ wrapper implementation
│   ├── lq_platform_arduino.cpp     # Arduino time/delay functions
│   └── platform_impl.cpp           # Platform-specific code selector
│
├── examples/                       # Arduino sketches (.ino files)
└── library.properties              # Arduino library metadata
```

## How It Works

### No Symlinks - Pure Relative Includes

All code is accessed via relative paths:

```cpp
// LayeredQueue.h includes headers from parent:
#include "../../include/lq_bldc.h"
#include "../../include/lq_j1939.h"
// ... etc

// LayeredQueue_Drivers.cpp includes source from parent:
#include "../../src/drivers/lq_bldc.c"
#include "../../src/drivers/lq_j1939.c"
// ... etc
```

**Benefits**:
- ✅ Works on Windows without special configuration
- ✅ No symlink setup needed
- ✅ ZIP distribution works perfectly
- ✅ Arduino IDE compiles files directly from parent paths
- ✅ Single source of truth - no code duplication

### Platform Implementations (via conditional include)

Platform-specific code is included from parent `src/platform/`:

```cpp
// platform_impl.cpp conditionally includes:
#if defined(__SAMD21__) || defined(__SAMD51__)
    #include "../../src/platform/lq_platform_samd.c"
#elif defined(ESP32)
    #include "../../src/platform/lq_platform_esp32.c"
#elif defined(STM32F4)
    #include "../../src/platform/lq_platform_stm32.c"
#endif
```

**Why include instead of separate files?**
- Platform selection happens at compile-time based on Arduino board
- Only one platform compiled per build
- No duplicate symbols
- All driver code in single compilation unit for better optimization

## Maintaining the Library

### When updating any driver (`src/drivers/*.c` or `include/*.h`)

✅ **No action needed** - relative includes automatically pick up changes

### When adding new driver

1. Create `src/drivers/lq_newdriver.c` and `include/lq_newdriver.h` in parent repo
2. Add include to `arduino/src/LayeredQueue_Drivers.cpp`:
   ```cpp
   #include "../../src/drivers/lq_newdriver.c"
   ```
3. Add include to `arduino/src/LayeredQueue.h`:
   ```cpp
   #include "../../include/lq_newdriver.h"
   ```

### When adding new platform

1. Create `src/platform/lq_platform_newplatform.c` in parent repo
2. Add include case in `arduino/src/platform_impl.cpp`:
The library is ready to distribute as-is:

```bash
# Create ZIP for Arduino Library Manager
cd arduino/
zip -r LayeredQueue-1.0.0.zip . -x ".*" -x "__*"
```

**Note**: The parent repository must be included since we use relative paths. The Arduino library is designed to be distributed as part of the full repository, not standalone.

## Installation Requirements

Users must:
1. Clone/download the **full repository** (not just `arduino/` folder)
2. Copy or symlink the `arduino/` folder to their Arduino libraries directory
3. The library will compile files from parent directories using relative includes

## Testing

The library shares code with the main repository:

1. **Unit tests** in `tests/` validate all drivers
2. **Platform implementations** tested in parent repo  
3. **Arduino wrapper** tested manually with examples

Run main repo tests:
```bash
cd /path/to/layered-queue-driver
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
make
./all_tests  # All 444+ tests
```

## Troubleshooting

### "lq_bldc.h: No such file or directory"

**Cause**: Arduino library installed without parent repository

**Fix**: 
1. Ensure full repository is available
2. Copy entire repo to Arduino libraries:
   ```bash
   cp -r /path/to/layered-queue-driver ~/Arduino/libraries/LayeredQueue
   ```
3. The `arduino/` folder must maintain its relative position to `src/` and `include/`

### "Multiple definition of..."

**Cause**: Files included multiple times

**Fix**: Check that `LayeredQueue_Drivers.cpp` only includes each `.c` file once

## File Size

The library compiles efficiently:
- Core drivers: ~40 KB compiled
- Platform implementation: ~10-15 KB (only one per build)
- BLDC wrapper: ~5 KB
- Total: ~55-70 KB depending on features used

Arduino compiler optimizes away unused functions

The Arduino library is very small:
- Core wrapper: ~5 KB
- Platform implementation: Compiled conditionally (~10-15 KB per platform)
- Examples: ~2 KB each
- Total compiled: ~20-30 KB (single platform)

Symlinks add no storage overhead.
