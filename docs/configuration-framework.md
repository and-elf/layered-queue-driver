# Configuration Framework Guide

## Overview

The configuration framework connects UDS (Unified Diagnostic Services) DIDs (Data Identifiers) to the layered queue's remap and scale drivers, enabling runtime configuration via diagnostic protocols.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  UDS Diagnostic Client                   │
│              (CANalyzer, Vector CANoe, etc.)            │
└────────────────────┬────────────────────────────────────┘
                     │ ISO-TP (CAN)
┌────────────────────▼────────────────────────────────────┐
│                   UDS Server (lq_uds.c)                  │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Service Handlers (0x10, 0x22, 0x2E, 0x27, 0x31) │  │
│  └────────────────────┬──────────────────────────────┘  │
└───────────────────────┼─────────────────────────────────┘
                        │ Callback Interface
┌───────────────────────▼─────────────────────────────────┐
│         Configuration Framework (lq_config.c)            │
│  ┌────────────────────────────────────────────────────┐ │
│  │            Configuration Registry                   │ │
│  │  - Remap array (LQ_MAX_REMAPS = 16)                │ │
│  │  - Scale array (LQ_MAX_SCALES = 16)                │ │
│  │  - Calibration mode state                          │ │
│  │  - Version tracking                                │ │
│  └────┬──────────────────────────────────────────┬────┘ │
└───────┼──────────────────────────────────────────┼──────┘
        │                                          │
┌───────▼────────────┐                  ┌─────────▼────────┐
│  Remap Drivers      │                  │  Scale Drivers   │
│  (lq_remap.c)       │                  │  (lq_scale.c)    │
│                     │                  │                  │
│  • Signal mapping   │                  │  • Linear xform  │
│  • Inversion        │                  │  • Clamping      │
│  • Deadzone filter  │                  │  • Offset        │
└────────┬────────────┘                  └────────┬─────────┘
         │                                        │
         └────────────────┬───────────────────────┘
                          │
                ┌─────────▼─────────┐
                │   Engine Core     │
                │  (lq_engine.c)    │
                │                   │
                │  32 Signals       │
                └───────────────────┘
```

## Data Identifiers (DIDs)

The configuration framework uses custom DIDs in the range `0xF1A0-0xF1FF`:

| DID    | Name                | Access | Description                           |
|--------|---------------------|--------|---------------------------------------|
| 0xF1A0 | SIGNAL_VALUE        | Read   | Read signal value (32-bit signed)     |
| 0xF1A1 | SIGNAL_STATUS       | Read   | Read signal status byte               |
| 0xF1A2 | REMAP_CONFIG        | R/W    | Remap configuration (5 bytes)         |
| 0xF1A3 | SCALE_CONFIG        | R/W    | Scale configuration (15 bytes)        |
| 0xF1A4 | FAULT_STATUS        | Read   | Fault status (reserved)               |
| 0xF1A5 | DTC_STATUS          | Read   | DTC status (reserved)                 |
| 0xF1A6 | CALIBRATION_MODE    | Read   | Calibration mode status (8 bytes)     |

### DID 0xF1A0: Read Signal Value

**Request Format:**
```
Byte 0: Signal ID (0-31)
```

**Response Format:**
```
Byte 0-3: Signal value (32-bit signed, big-endian)
```

**Example:**
```
Request:  22 F1 A0 05        (Read signal 5)
Response: 62 F1 A0 00 00 30 39  (Value = 12345 = 0x00003039)
```

### DID 0xF1A1: Read Signal Status

**Request Format:**
```
Byte 0: Signal ID (0-31)
```

**Response Format:**
```
Byte 0: Status (enum lq_event_status)
  0 = LQ_EVENT_OK
  1 = LQ_EVENT_DEGRADED
  2 = LQ_EVENT_OUT_OF_RANGE
  3 = LQ_EVENT_ERROR
  4 = LQ_EVENT_TIMEOUT
  5 = LQ_EVENT_INCONSISTENT
```

### DID 0xF1A2: Remap Configuration

**Serialization Format (5 bytes):**
```
Byte 0:   Input signal ID (0-31)
Byte 1:   Output signal ID (0-31)
Byte 2:   Flags
          Bit 0: Invert (0=normal, 1=inverted)
          Bit 1: Enabled (0=disabled, 1=enabled)
          Bits 2-7: Reserved
Byte 3-4: Deadzone (16-bit signed, big-endian)
```

**Read Request Format:**
```
Byte 0: Remap index (0-15)
```

**Write Request Format:**
```
Byte 0:   Remap index (0-15)
Byte 1-5: Remap configuration (see above)
```

**Example - Read Remap[0]:**
```
Request:  22 F1 A2 00
Response: 62 F1 A2 01 0A 03 00 32
          Input=1, Output=10, Invert=1, Enabled=1, Deadzone=50
```

**Example - Write Remap[0]:**
```
Request: 2E F1 A2 00 03 0B 02 00 64
         Index=0, Input=3, Output=11, Enabled=1, Deadzone=100
```

### DID 0xF1A3: Scale Configuration

**Serialization Format (15 bytes):**
```
Byte 0:    Input signal ID (0-31)
Byte 1:    Output signal ID (0-31)
Byte 2:    Flags
           Bit 0: Enabled
           Bit 1: Has clamp min
           Bit 2: Has clamp max
           Bits 3-7: Reserved
Byte 3-4:  Scale factor (16-bit signed, big-endian)
Byte 5-6:  Offset (16-bit signed, big-endian)
Byte 7-10: Clamp min (32-bit signed, big-endian)
Byte 11-14: Clamp max (32-bit signed, big-endian)
```

**Read Request Format:**
```
Byte 0: Scale index (0-15)
```

**Write Request Format:**
```
Byte 0:    Scale index (0-15)
Byte 1-15: Scale configuration (see above)
```

**Example - Write Scale[0] with 1.5x factor, clamp to [0, 8000]:**
```
Request: 2E F1 A3 00 01 0B 07 00 96 00 00 00 00 00 00 00 00 1F 40
         Index=0, Input=1, Output=11, Enabled|HasMin|HasMax
         Scale=150 (0x0096), Offset=0, Min=0, Max=8000 (0x1F40)
```

### DID 0xF1A6: Calibration Mode Status

**Response Format (8 bytes):**
```
Byte 0:   Calibration mode (0=inactive, 1=active)
Byte 1:   Config locked (0=unlocked, 1=locked)
Byte 2:   Number of remaps
Byte 3:   Number of scales
Byte 4-7: Configuration version (32-bit, big-endian)
```

## Routine Identifiers (RIDs)

Configuration management routines in range `0xF1A0-0xF1A2`:

| RID    | Name                | Description                           |
|--------|---------------------|---------------------------------------|
| 0xF1A0 | ENTER_CALIBRATION   | Enter calibration mode (unlock)       |
| 0xF1A1 | EXIT_CALIBRATION    | Exit calibration mode (lock)          |
| 0xF1A2 | RESET_DEFAULTS      | Reset all configs to defaults         |

### RID 0xF1A0: Enter Calibration Mode

Enables configuration modification. Required before writing DIDs 0xF1A2/0xF1A3.

**Request:**
```
31 01 F1 A0  (Routine Control, Start, RID=0xF1A0)
```

**Response:**
```
71 01 F1 A0  (Positive response)
```

### RID 0xF1A1: Exit Calibration Mode

Locks configuration and validates all parameters.

**Request:**
```
31 01 F1 A1
```

**Response:**
```
71 01 F1 A1
```

### RID 0xF1A2: Reset to Defaults

Clears all remap/scale configurations. Requires calibration mode.

**Request:**
```
31 01 F1 A2
```

**Response:**
```
71 01 F1 A2
```

## Security Model

Configuration writes require proper UDS security:

1. **Session Control**: Extended diagnostic session (0x10 0x03)
2. **Security Access**: Level 2 or higher (0x27 0x03/0x04)
3. **Calibration Mode**: Must be active (RID 0xF1A0)

**Without calibration mode:**
```
Request:  2E F1 A2 00 01 02 03 00 00
Response: 7F 2E 33  (NRC 0x33 = Security Access Denied)
```

## Typical Workflow

### 1. Enter Diagnostic Session
```
10 03              → Extended diagnostic session
50 03 00 32 01 F4  ← P2=50ms, P2*=5000ms
```

### 2. Unlock Security
```
27 03              → Request seed (level 2)
67 03 12 34 56 78  ← Seed = 0x12345678

27 04 AB CD EF 01  → Send key (calculated from seed)
67 04              ← Security unlocked
```

### 3. Enter Calibration Mode
```
31 01 F1 A0        → Start calibration routine
71 01 F1 A0        ← Success
```

### 4. Read Current Configuration
```
22 F1 A2 00        → Read Remap[0]
62 F1 A2 ...       ← Current values
```

### 5. Modify Configuration
```
2E F1 A2 00 ...    → Write new Remap[0]
6E F1 A2           ← Success
```

### 6. Exit Calibration
```
31 01 F1 A1        → Exit calibration
71 01 F1 A1        ← Config locked
```

## Error Handling

Common NRCs (Negative Response Codes):

| NRC  | Name                          | Cause                                 |
|------|-------------------------------|---------------------------------------|
| 0x13 | Incorrect Message Length      | Wrong DID data length                 |
| 0x31 | Request Out Of Range          | Invalid signal ID, index, or range    |
| 0x33 | Security Access Denied        | Not in calibration mode or no security|
| 0x72 | General Programming Failure   | Internal error                        |

**Example - Invalid signal ID:**
```
Request:  22 F1 A0 FF        (Signal 255 > max 31)
Response: 7F 22 31           (NRC 0x31 = Out of Range)
```

**Example - Write without calibration mode:**
```
Request:  2E F1 A2 00 ...
Response: 7F 2E 33           (NRC 0x33 = Security Denied)
```

## API Usage

### Initialize Configuration Registry

```c
#include "lq_config.h"

struct lq_engine engine;
struct lq_config_registry registry;
struct lq_remap_ctx remaps[16];
struct lq_scale_ctx scales[16];

/* Initialize */
lq_config_init(&registry, &engine, remaps, 16, scales, 16);
```

### Add Configuration

```c
/* Add remap */
struct lq_remap_ctx remap = {
    .input_signal = 0,
    .output_signal = 10,
    .invert = false,
    .deadzone = 50,
    .enabled = true,
};

uint8_t index;
lq_config_add_remap(&registry, &remap, &index);
```

### Connect to UDS Server

```c
#include "lq_uds.h"

/* UDS DID callbacks */
static int read_did(uint16_t did, uint8_t *data, size_t max_len, size_t *actual_len)
{
    return lq_config_uds_read_did(&registry, did, data, max_len, actual_len);
}

static int write_did(uint16_t did, const uint8_t *data, size_t len)
{
    return lq_config_uds_write_did(&registry, did, data, len);
}

static int routine_control(uint16_t rid, uint8_t control_type,
                           const uint8_t *in_data, size_t in_len,
                           uint8_t *out_data, size_t max_out, size_t *actual_out)
{
    return lq_config_uds_routine_control(&registry, rid, control_type,
                                        in_data, in_len, out_data, max_out, actual_out);
}

/* Configure UDS server */
struct lq_uds_config uds_config = {
    .read_did = read_did,
    .write_did = write_did,
    .routine_control = routine_control,
    /* ... other fields ... */
};
```

## Configuration Persistence

The framework provides in-memory configuration only. For persistent storage:

1. **Manual Save**: Serialize registry to EEPROM/Flash after changes
2. **UDS Programming Session**: Use 0x10 0x02 + flash services
3. **External Storage**: Export via custom DID (e.g., 0xF1B0)

**Example - Save to NVRAM:**
```c
void save_config_to_nvram(const struct lq_config_registry *registry)
{
    /* Save header */
    nvram_write(&registry->num_remaps, sizeof(uint8_t));
    nvram_write(&registry->num_scales, sizeof(uint8_t));
    nvram_write(&registry->config_version, sizeof(uint32_t));
    
    /* Save remap array */
    nvram_write(registry->remaps, 
                registry->num_remaps * sizeof(struct lq_remap_ctx));
    
    /* Save scale array */
    nvram_write(registry->scales,
                registry->num_scales * sizeof(struct lq_scale_ctx));
}
```

## Testing

See `tests/config_test.cpp` for comprehensive examples:

```bash
cd build
./all_tests --gtest_filter="ConfigTest.*"
```

All 22 configuration tests validate:
- Registry initialization
- Signal read access
- Remap add/read/write/remove
- Scale add/read/write with validation
- Calibration mode security
- UDS DID serialization/deserialization
- Routine control operations

## Performance

Configuration operations are lightweight:

- **DID Read**: O(1) array access, ~1µs
- **DID Write**: O(1) update + validation, ~2µs
- **Add/Remove**: O(n) with n ≤ 16, ~5µs worst case

ISO-TP transport adds ~10-50ms latency for multi-frame messages over CAN.

## Limitations

1. **Max Configurations**: 16 remaps, 16 scales (configurable via `LQ_MAX_REMAPS`/`LQ_MAX_SCALES`)
2. **Max Signals**: 32 (configurable via `LQ_MAX_SIGNALS`)
3. **No Atomicity**: Multi-parameter updates aren't atomic (use calibration mode)
4. **No History**: Version counter only, no undo/redo
5. **Memory**: Static allocation only (deterministic for safety)

## See Also

- [UDS Protocol](../include/lq_uds.h) - UDS server implementation
- [ISO-TP Transport](../include/lq_isotp.h) - CAN transport layer
- [Remap Driver](../include/lq_remap.h) - Signal remapping
- [Scale Driver](../include/lq_scale.h) - Linear scaling
- [Configuration Example](../samples/config_example.c) - Complete example
