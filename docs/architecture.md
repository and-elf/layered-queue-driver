# Layered Queue Driver Architecture

## Overview

The Layered Queue Driver provides a flexible, declarative framework for building robust data pipelines in safety-critical embedded systems. The architecture uses clean layering to separate hardware concerns from pure processing logic, enabling testability, portability, and deterministic behavior.

## Layered Architecture

The system is organized into distinct layers with clear responsibilities:

```
┌─────────────────────────────────────────────┐
│         Hardware ISR / Polling              │ ← RTOS-aware, minimal
│  (ADC callbacks, SPI reads, GPIO events)    │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_push()
┌─────────────────▼───────────────────────────┐
│      Layer 2: Input Aggregator              │ ← RTOS-aware, thin
│         (Hardware ringbuffer)               │
└─────────────────┬───────────────────────────┘
                  │ lq_hw_pop()
┌─────────────────▼───────────────────────────┐
│    Mid-level Drivers (vtable pattern)       │ ← PURE (no RTOS calls)
│  • ADC validator                            │
│  • SPI validator                            │
│  • Merge/Voter                              │
│  • Range checker                            │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│         ENGINE STEP (pure, once)            │ ← PURE (deterministic)
│    Process all inputs → Generate events     │
└─────────────────┬───────────────────────────┘
                  │ events
┌─────────────────▼───────────────────────────┐
│          Output Drivers                     │ ← Hardware adapters
│  • CAN                                      │
│  • GPIO                                     │
│  • UART/Serial                              │
└─────────────────┬───────────────────────────┘
                  │
┌─────────────────▼───────────────────────────┐
│       Sync Groups (atomic updates)          │
│    Coordinate multiple outputs together     │
└─────────────────────────────────────────────┘
```

### Layer Responsibilities

#### Layer 1: Hardware ISR / Polling
- **Responsibility**: Capture raw hardware samples as quickly as possible
- **RTOS-aware**: Yes (ISR-safe, uses ringbuffer)
- **Side effects**: Minimal - just capture and timestamp
- **Example**:
```c
void adc_isr_callback(uint16_t sample)
{
    lq_hw_push(LQ_HW_ADC0, sample);  // ISR-safe
}
```

#### Layer 2: Input Aggregator
- **Responsibility**: Buffer samples from all hardware sources
- **RTOS-aware**: Yes (mutex-protected ringbuffer)
- **Interface**: `lq_hw_push()` (ISR-safe) and `lq_hw_pop()` (thread-safe)
- **Data structure**: `struct lq_hw_sample` with source ID, value, timestamp

#### Layer 3: Mid-level Drivers
- **Responsibility**: Pure processing - validate, filter, merge
- **RTOS-aware**: NO (completely pure functions)
- **Interface**: Vtable pattern with `init()` and `process()` methods
- **Input**: Raw samples + timestamp
- **Output**: Validated events
- **Types**:
  - ADC validator: Range checking, staleness detection
  - SPI validator: Communication error detection
  - Merge/Voter: Redundancy management with median/average/min/max
  - Range checker: Multi-level range validation

#### Layer 4: Engine Step
- **Responsibility**: Single deterministic processing cycle
- **RTOS-aware**: NO (completely pure)
- **Process**:
  1. Ingest events into canonical signals
  2. Apply staleness detection
  3. Process merge/voter logic
  4. Generate on-change output events
  5. Process cyclic deadline-scheduled outputs
- **Guarantees**: Deterministic, testable, no side effects

**Engine Step Implementation:**
```c
void lq_engine_step(
    struct lq_engine *e,
    uint64_t now,
    const struct lq_event *events,
    size_t n_events)
{
    e->out_event_count = 0;
    
    // Phase 1: Update canonical signal values
    lq_ingest_events(e, events, n_events);
    
    // Phase 2: Check for stale data
    lq_apply_input_staleness(e, now);
    
    // Phase 3: Run merge/voter algorithms
    lq_process_merges(e, now);
    
    // Phase 4: Generate on-change outputs
    lq_process_outputs(e);
    
    // Phase 5: Generate cyclic outputs (deadline-based)
    lq_process_cyclic_outputs(e, now);
    
    // Output events ready in e->out_events[]
}
```

#### Layer 5: Output Drivers
- **Responsibility**: Adapt events to hardware interfaces
- **RTOS-aware**: Yes (calls hardware APIs)
- **Interface**: Vtable with `write()`, optional `stage()`/`commit()`
- **Types**:
  - CAN: Format events as CAN frames with PGN
  - GPIO: Drive pins based on event status
  - UART: Serialize events for transmission

#### Layer 6: Sync Groups
- **Responsibility**: Coordinate atomic updates across outputs
- **Use case**: Ensure CAN frames, GPIO, and other outputs update together
- **Pattern**: Stage → Commit for atomicity
- **Timing**: Configurable period (e.g., 100ms snapshots)

### Core Components

## Runtime Behavior

### Queue Operations

**Push Operation:**
1. Acquire mutex
2. Check capacity:
   - If full and policy is `drop-oldest`: remove oldest item
   - If full and policy is `drop-newest`: reject new item
   - If full and policy is `block`: wait on semaphore
3. Insert item at write index
4. Update write index (circular)
5. Signal read semaphore
6. Update statistics
7. Invoke callback if registered
8. Release mutex

**Pop Operation:**
1. Wait on read semaphore (with timeout)
2. Acquire mutex
3. Read item from read index
4. Update read index (circular)
5. Signal write semaphore (if blocking policy)
6. Update statistics
7. Release mutex

### ADC Source Driver

**Initialization:**
1. Parse device tree: ADC device, channel, queue, ranges
2. Allocate sample buffer for averaging
3. Initialize delayed work item
4. Schedule first poll

**Polling Cycle:**
1. Read ADC channel
2. Add to averaging buffer
3. If buffer full:
   - Compute average
   - Validate against ranges (first match wins)
   - Create `lq_item` with value, status, timestamp
   - Push to output queue
   - Reset buffer
4. Reschedule poll work

**Range Validation:**
- Iterate through ranges in order
- Return status of first range where `min <= value <= max`
- If no range matches, return `LQ_ERROR`

### SPI Source Driver

**Initialization:**
1. Parse device tree: SPI device, register, queue, expected values
2. Configure SPI transaction parameters
3. Initialize delayed work item
4. Schedule first poll

**Polling Cycle:**
1. Execute SPI read transaction
2. Parse response into integer value
3. Validate against expected values
4. Create `lq_item` with value, status, timestamp
5. Push to output queue
6. Reschedule poll work

**Value Validation:**
- Compare read value against expected values array
- Return status of matching expected value
- If no match, return `LQ_ERROR`

### Merge/Voter Driver

**Initialization:**
1. Parse device tree: input queues, output queue, method, tolerance, range
2. Allocate arrays for last values and timestamps
3. Initialize delayed work item
4. Schedule first merge cycle

**Merge Cycle:**
1. Pop latest item from each input queue (non-blocking)
2. Store values and timestamps
3. Check if all inputs are fresh (within timeout)
4. Perform voting algorithm:
   - **Median**: Sort values, select middle value
   - **Average**: Compute arithmetic mean
   - **Min/Max**: Select minimum or maximum
   - **Majority**: Require values within tolerance, use median
5. Check tolerance:
   - Compute max deviation from voted value
   - If deviation > tolerance, set `LQ_INCONSISTENT` status
6. Validate against expected range (if configured)
7. Create `lq_item` with voted value and status
8. Push to output queue
9. Reschedule merge work

**Consistency Checking:**
```c
max_deviation = 0;
for each input_value:
    deviation = abs(input_value - voted_value);
    max_deviation = max(max_deviation, deviation);

if (max_deviation > tolerance):
    status = LQ_INCONSISTENT or status_if_violation;
```

### Dual-Inverted Driver

**Initialization:**
1. Parse device tree: normal GPIO, inverted GPIO, queue, debounce
2. Configure GPIOs as inputs with pull-ups/pull-downs
3. Initialize delayed work item
4. Schedule first poll

**Polling Cycle:**
1. Read both GPIO pins
2. Check consistency:
   - **Valid states**: `normal=1 && inverted=0` → output 1
   - **Valid states**: `normal=0 && inverted=1` → output 0
   - **Error**: `normal=1 && inverted=1` (if enabled)
   - **Error**: `normal=0 && inverted=0` (if enabled)
3. Apply debounce:
   - If state changed, increment debounce counter
   - If stable for debounce_ms, accept new state
4. Create `lq_item` with state value and status
5. Push to output queue
6. Reschedule poll work

**Error Detection:**
- Both signals high or both low indicates wiring fault or sensor failure
- Configurable per device tree (some systems may allow both-low during power-up)

## Error Handling

### Source Errors
- **ADC read failure**: Push item with `LQ_ERROR` status, value = 0
- **SPI transaction failure**: Push item with `LQ_ERROR` status
- **Queue full (drop-newest policy)**: Increment drop counter, log warning

### Merge Errors
- **Input timeout**: Use stale data with `LQ_TIMEOUT` status
- **Inconsistent inputs**: Use voted value with `LQ_INCONSISTENT` status
- **Range violation**: Use value with `LQ_OUT_OF_RANGE` status

### Queue Errors
- **Pop timeout**: Return `-EAGAIN`
- **Invalid parameters**: Return `-EINVAL`
- **Queue not ready**: Return `-ENODEV`

## Thread Safety

- All queue operations protected by mutex
- Semaphores used for blocking/signaling
- Atomic operations for counters
- Source drivers use Zephyr work queue (serialized per driver)
- Callbacks executed in work queue context

## Performance Considerations

- Ring buffers avoid memory allocation during runtime
- Delayed work minimizes thread overhead
- Averaging reduces noise without filtering libraries
- Voting algorithms are O(n log n) for median, O(n) for others

## Device Tree Best Practices

1. **Define ranges from specific to general** (narrow ranges first)
2. **Set reasonable poll intervals** (faster = more CPU, slower = higher latency)
3. **Size queues appropriately** (capacity > poll_rate × consumer_latency)
4. **Use tight tolerances for safety-critical systems**
5. **Configure debounce for mechanical switches** (typically 5-50ms)

## Example Use Cases

### Automotive Brake System
- Dual ADC sensors for brake pressure
- Median voter with ±0.1 bar tolerance
- Range validation: 0-8 bar with warnings above 1.5 bar
- Queue capacity for 100ms of data at 100Hz polling

### Industrial Process Control
- SPI temperature sensor with expected states
- Dual-inverted emergency stop switch
- Merge ADC+SPI for critical temperature monitoring
- Out-of-range detection with automatic shutdown

### Aerospace Attitude Sensor
- Triple redundant IMU inputs
- Majority voter with strict tolerance
- Timeout detection for sensor failures
- Priority queues for real-time control loops
