# Diagnostic Trouble Code (DTC) System

## Overview

The Layered Queue Driver includes a production-ready Diagnostic Trouble Code (DTC) system for automotive fault reporting. It supports:

- **J1939 DM1** (PGN 65226): Active fault messages with lamp status
- **J1939 DM2** (PGN 65227): Stored/cleared fault history
- **ISO 14229 UDS** compatibility (future)
- Occurrence counting and fault state management
- MIL (Malfunction Indicator Lamp) priority handling

## Quick Start

### 1. Initialize DTC Manager

```c
#include "lq_dtc.h"

static struct lq_dtc_manager dtc_mgr;

void app_init(void) {
    lq_dtc_init(&dtc_mgr, 1000);  // 1000ms DM1 broadcast rate
}
```

### 2. Report Faults from Monitors

```c
// In your fault monitor callback
void monitor_rpm_sensor(struct lq_fault_monitor *fm, 
                        const struct lq_signal *sig,
                        void *user_data) {
    if (sig->value < 0 || sig->value > 8000) {
        // SPN 190 = Engine Speed
        // FMI 4 = Voltage Below Normal
        lq_dtc_set_active(&dtc_mgr, 190, LQ_FMI_VOLTAGE_BELOW_NORMAL,
                         LQ_LAMP_AMBER, lq_platform_now_us());
    }
}
```

### 3. Broadcast DM1 Messages

```c
void app_cyclic_1hz(void) {
    uint8_t dm1_msg[64];
    uint64_t now = lq_platform_now_us();
    
    int len = lq_dtc_build_dm1(&dtc_mgr, dm1_msg, sizeof(dm1_msg), now);
    if (len > 0) {
        j1939_send(0xFECA, dm1_msg, len);  // PGN 65226 (DM1)
    }
    
    // Update MIL based on highest severity fault
    enum lq_lamp_status mil = lq_dtc_get_mil_status(&dtc_mgr);
    update_dashboard_lamps(mil);
}
```

### 4. Handle DM2 Requests

```c
void handle_dm2_request(void) {
    uint8_t dm2_msg[64];
    
    int len = lq_dtc_build_dm2(&dtc_mgr, dm2_msg, sizeof(dm2_msg));
    if (len > 0) {
        j1939_send(0xFECB, dm2_msg, len);  // PGN 65227 (DM2)
    }
}
```

## J1939 Failure Mode Identifiers (FMI)

Standard J1939 FMI codes available:

```c
enum lq_fmi {
    LQ_FMI_DATA_VALID_ABOVE_NORMAL = 0,
    LQ_FMI_DATA_VALID_BELOW_NORMAL = 1,
    LQ_FMI_DATA_ERRATIC = 2,
    LQ_FMI_VOLTAGE_ABOVE_NORMAL = 3,
    LQ_FMI_VOLTAGE_BELOW_NORMAL = 4,
    LQ_FMI_CURRENT_BELOW_NORMAL = 5,
    LQ_FMI_CURRENT_ABOVE_NORMAL = 6,
    LQ_FMI_MECHANICAL_FAILURE = 7,
    LQ_FMI_ABNORMAL_FREQUENCY = 8,
    LQ_FMI_ABNORMAL_UPDATE_RATE = 9,
    LQ_FMI_ABNORMAL_RATE_OF_CHANGE = 10,
    LQ_FMI_ROOT_CAUSE_NOT_KNOWN = 12,
    LQ_FMI_OUT_OF_CALIBRATION = 13,
    LQ_FMI_SPECIAL_INSTRUCTIONS = 14,
    // ... see lq_dtc.h for full list
};
```

## Lamp Status Priority

Lamp severity (highest to lowest):

1. **RED** (`LQ_LAMP_RED`): Critical faults, stop engine
2. **AMBER_FLASH** (`LQ_LAMP_AMBER_FLASH`): Severe warning
3. **AMBER** (`LQ_LAMP_AMBER`): Standard warning
4. **OFF** (`LQ_LAMP_OFF`): No active faults

The system automatically reports the highest severity lamp across all active DTCs.

## DTC Lifecycle

```
INACTIVE → PENDING → CONFIRMED → STORED
             ↑          |           ↑
             └─────────┘           │
                  (clear)──────────┘
```

- **INACTIVE**: No fault present
- **PENDING**: Fault detected but not yet confirmed (future use)
- **CONFIRMED**: Fault confirmed and active
- **STORED**: Fault cleared but stored in history

## API Reference

### Initialization

```c
void lq_dtc_init(struct lq_dtc_manager *mgr, uint16_t dm1_period_ms);
```

### Fault Management

```c
// Report active fault
int lq_dtc_set_active(struct lq_dtc_manager *mgr, uint32_t spn, 
                      uint8_t fmi, enum lq_lamp_status lamp, uint64_t now);

// Clear fault (moves to STORED state)
int lq_dtc_clear(struct lq_dtc_manager *mgr, uint32_t spn, 
                 uint8_t fmi, uint64_t now);

// Clear all faults
void lq_dtc_clear_all(struct lq_dtc_manager *mgr);
```

### Status Queries

```c
// Get count of active DTCs
uint8_t lq_dtc_get_active_count(const struct lq_dtc_manager *mgr);

// Get count of stored DTCs
uint8_t lq_dtc_get_stored_count(const struct lq_dtc_manager *mgr);

// Get highest severity lamp status
enum lq_lamp_status lq_dtc_get_mil_status(const struct lq_dtc_manager *mgr);
```

### Message Building

```c
// Build J1939 DM1 message (rate-limited)
int lq_dtc_build_dm1(struct lq_dtc_manager *mgr, uint8_t *data, 
                     size_t max_size, uint64_t now);

// Build J1939 DM2 message
int lq_dtc_build_dm2(struct lq_dtc_manager *mgr, uint8_t *data, 
                     size_t max_size);
```

## Configuration

```c
// Maximum DTCs (default: 32)
#define LQ_MAX_DTCS 32
```

## Common SPNs for Automotive Systems

| SPN  | Description                    |
|------|--------------------------------|
| 84   | Wheel-Based Vehicle Speed      |
| 91   | Accelerator Pedal Position     |
| 94   | Fuel Delivery Pressure         |
| 100  | Engine Oil Pressure            |
| 110  | Engine Coolant Temperature     |
| 168  | Battery Voltage                |
| 190  | Engine Speed (RPM)             |
| 512  | Driver's Demand Engine Torque  |
| 1127 | Transmission Oil Temperature   |
| 1637 | Brake Pedal Position           |

See SAE J1939-71 for complete SPN list.

## DM1 Message Format

```
Byte 0-1: Protect Lamp Status
Byte 2-3: Amber/Red Lamp Status
Byte 4+:  DTCs (4 bytes each)

DTC Format:
  Byte 0: SPN bits 0-7
  Byte 1: SPN bits 8-15
  Byte 2: SPN bits 16-18 (upper 3 bits) | FMI (lower 5 bits)
  Byte 3: Occurrence count
```

## Integration with Fault Monitors

```c
// Example: Temperature sensor monitoring
static void temp_monitor_callback(struct lq_fault_monitor *fm,
                                  const struct lq_signal *sig,
                                  void *user_data) {
    struct lq_dtc_manager *dtc = (struct lq_dtc_manager *)user_data;
    uint64_t now = lq_platform_now_us();
    
    if (sig->value > 120) {
        // Engine overheating - RED lamp
        lq_dtc_set_active(dtc, 110, LQ_FMI_DATA_VALID_ABOVE_NORMAL,
                         LQ_LAMP_RED, now);
        
        // Trigger limp-home mode
        fm->limp_home = true;
        fm->limp_home_value = 50;  // Limit to 50°C
    }
    else if (sig->error_status != LQ_ERROR_NONE) {
        // Sensor fault - AMBER lamp
        lq_dtc_set_active(dtc, 110, LQ_FMI_DATA_ERRATIC,
                         LQ_LAMP_AMBER, now);
    }
}

// Configure in devicetree:
fault_monitor_temp: monitor@2 {
    compatible = "layered-queue,fault-monitor";
    source = <&temp_merge>;
    min-value = <-40>;
    max-value = <130>;
    timeout-us = <500000>;
    wake-function = <temp_monitor_callback>;
    user-data = <&dtc_mgr>;
};
```

## Best Practices

1. **Rate Limiting**: DM1 broadcasts are automatically rate-limited to configured period (default 1000ms)

2. **Occurrence Counting**: DTCs automatically track occurrence count (max 255). Re-setting an active DTC increments the counter.

3. **Lamp Priority**: Always set appropriate lamp severity:
   - RED: Safety-critical faults
   - AMBER_FLASH: Severe degradation
   - AMBER: Standard warnings

4. **Clear vs. Clear All**:
   - Use `lq_dtc_clear()` for specific fault resolution
   - Use `lq_dtc_clear_all()` only for service resets

5. **Memory Management**: Maximum `LQ_MAX_DTCS` (32) can be active or stored simultaneously

6. **Integration**: Pass DTC manager to fault monitor wake functions via `user_data` pointer

## Testing

Run DTC unit tests:

```bash
./all_tests --gtest_filter="DTCTest.*"
```

Tests cover:
- Initialization and lifecycle
- Occurrence counting
- Lamp priority logic
- DM1/DM2 message encoding
- Rate limiting
- Edge cases (max DTCs, clearing nonexistent)

## Future Enhancements

- ISO 14229 (UDS) DTC format support
- DTC aging and automatic clearing
- Snapshot data storage
- PENDING state with confirmation logic
- Non-volatile storage integration
- OBD-II P-codes mapping
