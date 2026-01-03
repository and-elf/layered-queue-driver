# Layered Architecture Migration Guide

This document explains the new layered architecture and how it improves upon the original queue-based design.

## Key Concept: Separation of Concerns

The new architecture separates the system into **pure** and **impure** layers:

### Pure Layers (No RTOS, Easily Testable)
- **Mid-level drivers**: Process raw samples → validated events
- **Engine step**: Coordinate all processing in one deterministic cycle

### Impure Layers (Hardware/RTOS interaction)
- **Hardware ISR/Polling**: Capture raw samples (ISR-safe)
- **Input aggregator**: Ringbuffer for sample collection
- **Output drivers**: Adapt events to CAN/GPIO/UART hardware
- **Sync groups**: Coordinate atomic hardware updates

## Architecture Comparison

### Old Design (Queue-based)
```
ADC ISR → Queue → Poll Thread → Validate → Merge Queue → App
SPI ISR → Queue → Poll Thread → Validate → Merge Queue → App
```

**Issues:**
- Mixing hardware and processing concerns
- Multiple queues and threads
- Hard to test (RTOS dependencies throughout)
- Non-deterministic timing

### New Design (Layered)
```
ADC ISR ────────┐
               ├──→ Ringbuffer ──→ Engine Step ──→ Events ──→ CAN/GPIO
SPI ISR ────────┘     (thin)         (PURE)                    (adapters)
```

**Benefits:**
- Clear layer boundaries
- Single processing point (deterministic)
- Pure functions easily tested
- Hardware abstraction

## Usage Example

### Hardware Layer
```c
// ISR captures raw sample
void adc_isr(uint16_t sample) {
    lq_hw_push(LQ_HW_ADC0, sample);  // O(1), ISR-safe
}
```

### Mid-level Driver (PURE)
```c
// Pure function: no RTOS calls, no side effects
size_t adc_process(struct lq_mid_driver *drv, 
                   uint64_t now,
                   const void *raw,
                   struct lq_event *out,
                   size_t max)
{
    const struct lq_hw_sample *sample = raw;
    struct lq_mid_adc_ctx *ctx = drv->ctx;
    
    // Validate range
    if (sample->value < ctx->min_raw || 
        sample->value > ctx->max_raw) {
        out->status = LQ_EVENT_OUT_OF_RANGE;
    } else {
        out->status = LQ_EVENT_OK;
    }
    
    out->value = sample->value;
    out->timestamp = sample->timestamp;
    return 1;  // Generated 1 event
}
```

### Engine Step (PURE)
```c
int lq_engine_step(struct lq_engine *engine, uint64_t now)
{
    struct lq_hw_sample sample;
    struct lq_event events[MAX_EVENTS];
    size_t num_events = 0;
    
    // Process all pending samples through mid-level drivers
    while (lq_hw_pop(&sample) == 0) {
        struct lq_mid_driver *drv = find_driver(sample.src);
        num_events += drv->v->process(drv, now, &sample, 
                                      &events[num_events],
                                      MAX_EVENTS - num_events);
    }
    
    // Execute engine step with collected events
    lq_engine_step(engine, now, events, num_events);
    
    // Output events are now in engine->out_events[]
    // Send to hardware in calling layer
    return num_events;
}

// Internal implementation
void lq_engine_step(
    struct lq_engine *e,
    uint64_t now,
    const struct lq_event *events,
    size_t n_events)
{
    e->out_event_count = 0;
    
    lq_ingest_events(e, events, n_events);
    lq_apply_input_staleness(e, now);
    lq_process_merges(e, now);
    lq_process_outputs(e);
    lq_process_cyclic_outputs(e, now);
    
    // e->out_events[] now contains all output events ready for transmission
}
```

### Output Driver
```c
// Adapts event to CAN hardware
int can_write(struct lq_output_driver *out, const struct lq_event *evt)
{
    struct lq_output_can_ctx *ctx = out->ctx;
    
    // Format CAN frame
    struct can_frame frame = {
        .id = ctx->pgn,
        .data = { evt->value >> 24, evt->value >> 16, 
                  evt->value >> 8, evt->value },
        .dlc = 4
    };
    
    return can_send(ctx->can_dev, &frame);
}
```

### Sync Group (Atomic Updates)
```c
// Update CAN + GPIO together every 100ms
struct lq_sync_group snapshot = {
    .period_us = 100000,
    .members = { &can_speed, &gpio_warning },
    .num_members = 2,
    .use_staging = true  // Stage then commit for atomicity
};

// In engine task
if (lq_sync_group_is_due(&snapshot, now)) {
    lq_sync_group_update(&snapshot, now, events, num_events);
}
```

### Cyclic Outputs (Deadline Scheduling)
```c
// Send J1939 engine speed every 100ms with deadline scheduling
struct lq_cyclic_ctx rpm_cyclic = {
    .type = LQ_OUTPUT_J1939,
    .target_id = 0xFEF1,          // Engine Speed PGN
    .source_signal = 100,         // Index into engine->signals[]
    .period_us = 100000,          // 100ms
    .next_deadline = 0,
    .flags = 0
};

// In engine step - process all cyclic outputs
for (size_t i = 0; i < engine->num_cyclic_outputs; i++) {
    lq_cyclic_process(engine, engine->cyclic_outputs[i], now);
}

// Output events are now in engine->out_events[]
// Send them to hardware
for (size_t i = 0; i < engine->out_event_count; i++) {
    struct lq_output_event *evt = &engine->out_events[i];
    switch (evt->type) {
        case LQ_OUTPUT_J1939:
            j1939_send(evt->target_id, evt->value);
            break;
        case LQ_OUTPUT_GPIO:
            gpio_pin_set(evt->target_id, evt->value);
            break;
    }
}
```

## Device Tree Pattern

```dts
/ {
    // Layer 1: Hardware sources
    inputs {
        adc0: lq-adc@0 { 
            compatible = "lq,adc";
            channel = <0>;
            stale-us = <5000>;
        };
    };
    
    // Layer 3: Mid-level processing
    merges {
        sensor: lq-merge@0 {
            inputs = <&adc0 &spi0>;
            voting-method = "median";
            signal-index = <100>;  // Maps to engine->signals[100]
        };
    };
    
    // Layer 5: Output adapters
    outputs {
        // Cyclic output: deadline-scheduled periodic transmission
        can0: lq-cyclic-output@0 {
            compatible = "lq,cyclic-output";
            source-signal = <100>;    // engine->signals[100]
            output-type = "j1939";
            target-id = <0xFEF1>;     // PGN
            period-us = <100000>;     // 10Hz
        };
        
        // On-change output: transmit when value changes
        gpio0: lq-output@0 {
            compatible = "lq,gpio-output";
            source-signal = <100>;
            pin = <5>;
            trigger-status = <1>;     // When status >= DEGRADED
        };
    };
    
    // Layer 6: Sync coordination (for on-change outputs)
    sync_groups {
        snapshot: lq-sync-group@0 {
            period-us = <100000>;
            members = <&gpio0>;
        };
    };
};
```

## Testing Strategy

### Pure Layers (Easy)
```c
TEST(MidDriver, ValidatesRange) {
    struct lq_mid_driver drv = { .v = &adc_vtbl, .ctx = &ctx };
    struct lq_hw_sample sample = { .value = 2048 };
    struct lq_event event;
    
    size_t n = adc_vtbl.process(&drv, 1000, &sample, &event, 1);
    
    EXPECT_EQ(n, 1);
    EXPECT_EQ(event.status, LQ_EVENT_OK);
    EXPECT_EQ(event.value, 2048);
}
```

### Engine Step (Easy)
```c
TEST(Engine, ProcessesSamples) {
    // Inject known samples
    lq_hw_push(LQ_HW_ADC0, 2048);
    lq_hw_push(LQ_HW_SPI0, 2050);
    
    // Run engine
    int n = lq_engine_step(&engine, 1000);
    
    // Verify events generated
    EXPECT_EQ(n, 2);
}
```

## Migration Path

The old queue-based system remains for backward compatibility. To migrate:

1. **Phase 1**: Add hardware layer calls (`lq_hw_push`)
2. **Phase 2**: Convert processing to mid-level drivers
3. **Phase 3**: Add output drivers
4. **Phase 4**: Remove old queues

No breaking changes required - both systems can coexist.

## Performance Characteristics

| Metric | Old (Queue) | New (Layered) |
|--------|-------------|---------------|
| ISR overhead | Queue push + semaphore | Ringbuffer write (faster) |
| Processing latency | Multiple queue hops | Single engine step |
| Determinism | Multiple threads | Single point |
| Memory | N queues × capacity | 1 ringbuffer |
| Testability | Hard (RTOS deps) | Easy (pure functions) |

## Summary

The layered architecture provides:
- ✅ **Cleaner separation** of hardware vs. logic
- ✅ **Pure processing layer** for easy testing
- ✅ **Deterministic behavior** with single engine step
- ✅ **Flexible outputs** with vtable pattern
- ✅ **Atomic coordination** via sync groups
- ✅ **Backward compatible** with legacy queue system
