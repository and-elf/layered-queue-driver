# Arduino Library Build Notes

## Architecture

The Arduino library is **thin wrapper** around the main repository code:

```
arduino/
├── src/
│   ├── LayeredQueue_BLDC.cpp      # Arduino C++ wrapper class
│   ├── LayeredQueue_BLDC.h        # Public API
│   ├── lq_platform_arduino.cpp    # Arduino time/delay functions
│   ├── platform_impl.cpp          # Includes parent platform code
│   ├── lq_bldc.c -> ../../src/drivers/lq_bldc.c    # Symlink to core
│   ├── lq_bldc.h -> ../../include/lq_bldc.h        # Symlink to header
│   └── lq_platform.h -> ../../include/lq_platform.h # Symlink
│
├── examples/                      # Arduino sketches (.ino files)
└── library.properties             # Arduino library metadata
```

## How It Works

### Shared Code (via symlinks)

The core BLDC driver and headers are **symlinked** from the main repository:
- `lq_bldc.c` → `../../src/drivers/lq_bldc.c` (core motor logic)
- `lq_bldc.h` → `../../include/lq_bldc.h` (API definitions)
- `lq_platform.h` → `../../include/lq_platform.h` (platform abstraction)

**Why symlinks?**
- Single source of truth - no code duplication
- Changes to core driver automatically available to Arduino users
- Easier maintenance

**Symlink compatibility:**
- ✅ Linux/macOS: Native support
- ✅ Windows: Works with Developer Mode or git config
- ✅ Arduino IDE: Follows symlinks during compilation
- ⚠️ ZIP distribution: Some unzip tools may not preserve symlinks

### Platform Implementations (via include)

Platform-specific code is **included** from parent `src/platform/`:

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

**Why include instead of symlink?**
- Platform selection happens at compile-time based on Arduino board
- Only one platform compiled per build
- No duplicate symbols
- Cleaner than conditional compilation in symlinked files

## Maintaining the Library

### When updating core driver (`src/drivers/lq_bldc.c`)

✅ **No action needed** - symlink automatically picks up changes

### When updating platform code (`src/platform/lq_platform_*.c`)

✅ **No action needed** - included via `platform_impl.cpp`

### When adding new platform

1. Create `src/platform/lq_platform_newplatform.c` in parent repo
2. Add include case in `arduino/src/platform_impl.cpp`:
   ```cpp
   #elif defined(NEW_PLATFORM_DEFINE)
       #include "../../src/platform/lq_platform_newplatform.c"
   ```
3. Update `library.properties` architectures field

### When releasing library

For best compatibility, consider creating a release package that resolves symlinks:

```bash
# Create standalone copy for distribution
cd arduino/
mkdir -p release/src
cp -L src/*.h release/src/    # -L follows symlinks
cp -L src/*.cpp release/src/
cp -L src/*.c release/src/
cp -r examples/ release/
cp *.md *.txt *.properties release/
```

## Testing

The Arduino library shares the same core code as the main repository, so:

1. **Unit tests** in `tests/bldc_test.cpp` validate the core driver
2. **Platform implementations** are tested in parent repo
3. **Arduino wrapper** is tested manually with example sketches

Run main repo tests:
```bash
cd /path/to/layered-queue-driver
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
make
./all_tests
```

## Windows Symlink Setup

If symlinks don't work on Windows:

### Option 1: Enable Developer Mode (Windows 10+)
Settings → Update & Security → For Developers → Developer Mode

### Option 2: Configure Git
```bash
git config --global core.symlinks true
```

Then re-clone the repository.

### Option 3: Manual Copy (Not Recommended)
If symlinks completely fail, manually copy files:
```bash
cd arduino/src
cp ../../src/drivers/lq_bldc.c .
cp ../../include/lq_bldc.h .
cp ../../include/lq_platform.h .
```

**Warning**: You must manually re-copy when upstream files change!

## Troubleshooting

### "lq_bldc.c not found"

**Cause**: Symlinks not working

**Fix**:
1. Check if symlinks exist: `ls -la arduino/src/`
2. Enable Windows Developer Mode or git symlinks
3. As last resort, manually copy files

### "Multiple definition of lq_bldc_init"

**Cause**: Platform file included multiple times

**Fix**: Check `platform_impl.cpp` has proper `#elif` structure

### "Unsupported platform warning"

**Cause**: Arduino board not recognized

**Fix**: Add platform detection in `platform_impl.cpp`:
```cpp
#elif defined(YOUR_BOARD_DEFINE)
    #include "../../src/platform/lq_platform_yourboard.c"
```

## File Size

The Arduino library is very small:
- Core wrapper: ~5 KB
- Platform implementation: Compiled conditionally (~10-15 KB per platform)
- Examples: ~2 KB each
- Total compiled: ~20-30 KB (single platform)

Symlinks add no storage overhead.
