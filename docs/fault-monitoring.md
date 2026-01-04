# Fault Monitoring and Safety Concept

## Overview

The layered queue driver now supports **declarative fault monitoring** to detect and respond to safety-critical conditions. Fault monitors observe signals and set fault flags when conditions are violated.

## Fault Monitor Node

Fault monitors are defined in the devicetree using `lq,fault-monitor`:

```dts
sensor_fault: lq-fault-monitor@0 {
    compatible = "lq,fault-monitor";
    status = "okay";
    
    input-signal-id = <10>;           /* Signal to monitor */
    fault-output-signal-id = <20>;    /* Fault flag output */
    
    /* Enable fault detection modes */
    check-staleness;
    stale-timeout-us = <50000>;       /* 50ms timeout */
    
    check-range;
    min-value = <0>;
    max-value = <5000>;
    
    check-status;  /* Detect merge/voting failures */
};
```

## Fault Detection Modes

### 1. Staleness Detection
Detects when a signal hasn't updated within the specified timeout:

```dts
check-staleness;
stale-timeout-us = <100000>;  /* 100ms */
```

Use case: Detect sensor communication failures

### 2. Range Checking
Detects when signal values exceed acceptable bounds:

```dts
check-range;
min-value = <-40>;   /* -40°C */
max-value = <125>;   /* 125°C */
```

Use case: Detect sensor malfunction or out-of-spec conditions

### 3. Status Checking
Detects fault conditions from upstream processing (e.g., voting inconsistencies):

```dts
check-status;
```

Use case: Detect when redundant sensors disagree beyond tolerance

## Complete Example

```dts
/ {
    /* Redundant temperature sensors */
    temp_a: lq-hw-adc-input@0 {
        signal-id = <0>;
        stale-us = <10000>;
    };
    
    temp_b: lq-hw-adc-input@1 {
        signal-id = <1>;
        stale-us = <10000>;
    };
    
    /* Merge with median voting */
    temp_merged: lq-mid-merge@0 {
        output-signal-id = <10>;
        input-signal-ids = <0 1>;
        voting-method = "median";
        tolerance = <5>;  /* 0.5°C tolerance */
    };
    
    /* Monitor for sensor faults */
    temp_fault: lq-fault-monitor@0 {
        input-signal-id = <10>;
        fault-output-signal-id = <20>;
        
        /* Detect stale data */
        check-staleness;
        stale-timeout-us = <50000>;
        
        /* Detect out-of-range */
        check-range;
        min-value = <-40>;
        max-value = <125>;
        
        /* Detect voting failures */
        check-status;
    };
    
    /* Output fault flag on CAN */
    fault_output: lq-cyclic-j1939-output@0 {
        source-signal-id = <20>;
        target-id = <0xFECA>;  /* Diagnostic message PGN */
        period-us = <1000000>; /* 1 Hz */
    };
};
```

## Fault Output Signal

The fault output signal has two states:
- **0**: No fault detected (OK)
- **1**: Fault condition active

This signal can be:
- Transmitted via CAN/J1939 for diagnostics
- Used by application logic for safety actions
- Combined with other fault signals using merge nodes

## Processing Order

Fault monitors run after merges in the engine processing pipeline:

1. **Ingest events** - New sensor data arrives
2. **Apply staleness** - Mark old signals as stale
3. **Process merges** - Run voting algorithms
4. **Process fault monitors** ← Fault detection happens here
5. **Process outputs** - Generate CAN messages

This ensures fault monitors can detect both individual sensor failures and merge/voting inconsistencies.

## Safety Applications

### Example: Engine Protection

```dts
/* Monitor critical engine parameters */
temp_monitor: lq-fault-monitor@0 {
    input-signal-id = <TEMP_MERGED>;
    fault-output-signal-id = <TEMP_FAULT>;
    check-range;
    max-value = <115>;  /* Overheat threshold */
};

oil_monitor: lq-fault-monitor@1 {
    input-signal-id = <OIL_PRESSURE>;
    fault-output-signal-id = <OIL_FAULT>;
    check-range;
    min-value = <20>;  /* Low pressure threshold */
};
```

Application code can then check fault signals and take action:
- Reduce engine power
- Enable limp-home mode
- Trigger warning indicators

## Benefits

✅ **Declarative** - Define fault conditions in devicetree, not C code  
✅ **Composable** - Combine multiple fault conditions  
✅ **Testable** - HIL tests can inject faults and verify detection  
✅ **Deterministic** - Fixed execution time, no dynamic allocation  
✅ **Safety-ready** - Structured approach for ISO 26262 / IEC 61508
