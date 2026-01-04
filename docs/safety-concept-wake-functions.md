# Safety Concept: Wake Functions

## Philosophy

**"Detect declaratively, respond immediately"**

The layered queue driver's safety concept is built on **wake functions** - user-defined callbacks that execute the moment a fault is detected. This enables safety-critical systems to respond in real-time without polling or delays.

## The Problem

Traditional approaches to fault handling:
- ❌ **Polling** - Application checks fault flags periodically (adds latency)
- ❌ **Interrupts** - Hard to configure, platform-specific, non-portable
- ❌ **Manual checking** - Easy to forget, error-prone, not systematic

## The Solution: Wake Functions

```
Fault Detected → Wake Function Called → Immediate Action Taken
     ↓                     ↓                      ↓
  (same cycle)      (user code)          (safety response)
```

### How It Works

1. **Declare** fault conditions in devicetree:
   ```dts
   temp_fault: lq-fault-monitor@0 {
       check-range;
       max-value = <115>;  /* °C */
       wake-function = "overheat_wake";
   };
   ```

2. **Implement** safety response in C:
   ```c
   void overheat_wake(uint8_t id, int32_t temp, bool fault) {
       if (fault) {
           emergency_shutdown();
           log_critical_event();
       }
   }
   ```

3. **Automatic** detection and response:
   - Engine runs fault monitors every cycle
   - Detects fault condition (temp > 115°C)
   - Immediately calls `overheat_wake()`
   - User code takes safety action
   - All within deterministic time bounds

## Key Characteristics

### Immediate
Wake functions execute **in the same processing cycle** as fault detection. No waiting, no polling, no delay.

### Deterministic
Fixed execution order:
1. Ingest sensor data
2. Apply staleness checks
3. Process merges/voting
4. **Check fault monitors** ← Wake functions called here
5. Process outputs

### Type-Safe
Generated code ensures correct signatures:
```c
// In lq_generated.h
void overheat_wake(uint8_t monitor_id, int32_t input_value, bool fault_detected);
```

Compiler errors if signature is wrong.

### Gradual
Weak stubs allow compilation even without implementation:
```c
// Auto-generated weak stub
__attribute__((weak))
void overheat_wake(uint8_t id, int32_t value, bool fault) {
    /* Default: no action */
}
```

Override when ready with actual safety logic.

## Safety Compliance

Supports **ISO 26262** (automotive) and **IEC 61508** (industrial):

✅ **Deterministic** - Bounded execution time  
✅ **Traceable** - DTS config links to code  
✅ **Testable** - HIL can inject faults, verify wake calls  
✅ **Documented** - Auto-generated from devicetree  
✅ **Reviewable** - Clear separation of detection (DTS) and response (C)

## Comparison

| Approach | Latency | Deterministic | Portable | Type-Safe |
|----------|---------|---------------|----------|-----------|
| Polling | High (~ms) | ❌ Depends on loop | ✅ | ❌ |
| Interrupts | Low (~µs) | ⚠️ Can preempt | ❌ Platform-specific | ❌ |
| **Wake Functions** | **Minimal** | **✅ Fixed** | **✅ Pure C** | **✅ Generated** |

## Real-World Example

**Automotive Engine Controller:**

```dts
oil_pressure_monitor: lq-fault-monitor@0 {
    input-signal-id = <OIL_PRESSURE>;
    fault-output-signal-id = <OIL_FAULT>;
    check-range;
    min-value = <20>;  /* 20 PSI minimum */
    check-staleness;
    stale-timeout-us = <100000>;  /* 100ms */
    wake-function = "oil_pressure_fault_wake";
};

coolant_temp_monitor: lq-fault-monitor@1 {
    input-signal-id = <COOLANT_TEMP>;
    fault-output-signal-id = <TEMP_FAULT>;
    check-range;
    max-value = <105>;  /* 105°C maximum */
    wake-function = "coolant_overheat_wake";
};
```

**Safety handlers:**

```c
void oil_pressure_fault_wake(uint8_t id, int32_t psi, bool fault)
{
    if (fault) {
        /* CRITICAL: Immediate action required */
        reduce_engine_rpm(800);      /* Idle speed only */
        disable_turbo();
        set_warning_lamp(OIL_LAMP);
        log_dtc(DTC_LOW_OIL_PRESSURE);
        
        if (psi < 10) {
            /* SEVERE: Complete shutdown */
            initiate_controlled_shutdown();
        }
    } else {
        /* Pressure restored - cautious recovery */
        if (engine_rpm < 1000 && psi > 25) {
            clear_warning_lamp(OIL_LAMP);
            enable_turbo();
        }
    }
}

void coolant_overheat_wake(uint8_t id, int32_t temp, bool fault)
{
    if (fault) {
        reduce_power_output(50);     /* 50% power limit */
        enable_cooling_fans(100);    /* Max fans */
        set_warning_lamp(TEMP_LAMP);
        
        if (temp > 110) {
            /* Emergency mode */
            reduce_power_output(25);
            request_driver_attention();
        }
    } else {
        /* Gradually restore normal operation */
        restore_power_output();
        set_cooling_fans_auto();
        clear_warning_lamp(TEMP_LAMP);
    }
}
```

**Result:**
- Fault detected within 5ms (one processing cycle)
- Wake function executes immediately
- Safety action taken before damage occurs
- Diagnostic codes logged
- Driver alerted
- System recovers gracefully when conditions improve

## Summary

Wake functions are the **core safety concept** of the layered queue driver:

1. **Detect** faults using declarative DTS configuration
2. **Wake** user code immediately when faults occur
3. **Respond** with safety-critical actions in bounded time
4. **Recover** gracefully when faults clear

This architecture enables safety-critical embedded systems to respond to faults faster than traditional polling or interrupt-based approaches, while remaining portable, testable, and compliant with functional safety standards.
