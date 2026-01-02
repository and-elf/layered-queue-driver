# Testing and Verification Plan

## Overview

Comprehensive testing strategy for the Layered Queue Driver covering unit tests, integration tests, hardware-in-the-loop tests, and safety validation.

## Test Levels

### 1. Unit Tests

**Location**: `tests/util/`

**Coverage**:
- ✓ Range validation logic
- ✓ Value validation logic
- ✓ Voting algorithms (median, average, min, max, majority)
- ✓ Tolerance checking
- ✓ Edge cases (empty ranges, NULL parameters)

**Run**:
```bash
west build -b native_posix tests/util
west build -t run
```

**Expected Results**:
- All validation functions return correct status codes
- Voting produces correct results for known inputs
- Tolerance violations detected correctly

### 2. Queue Tests

**Location**: `tests/queue/`

**Test Cases**:
1. **Basic Operations**
   - Push single item
   - Pop single item
   - Peek without removing
   - Empty queue pop returns -EAGAIN

2. **Capacity Management**
   - Fill to capacity
   - Drop-oldest policy: oldest item removed
   - Drop-newest policy: new item rejected
   - Block policy: push blocks until space

3. **Thread Safety**
   - Multiple producers, single consumer
   - Single producer, multiple consumers
   - Concurrent push/pop operations
   - Race condition detection

4. **Statistics**
   - Items written counter
   - Items read counter
   - Items dropped counter
   - Peak usage tracking

**Device Tree**:
```dts
/ {
    test_queue_small: lq-queue@0 {
        compatible = "zephyr,lq-queue";
        capacity = <4>;
        drop-policy = "drop-oldest";
    };

    test_queue_block: lq-queue@1 {
        compatible = "zephyr,lq-queue";
        capacity = <4>;
        drop-policy = "block";
    };
};
```

### 3. Source Driver Tests

**Location**: `tests/sources/`

**ADC Source Tests**:
1. **Range Validation**
   - Value in valid range → LQ_OK
   - Value in degraded range → LQ_DEGRADED
   - Value out of all ranges → LQ_OUT_OF_RANGE
   - First-match rule enforcement

2. **Averaging**
   - Single sample (averaging=1)
   - Multiple samples (averaging=4, 8)
   - Buffer wraparound

3. **Polling**
   - Correct interval timing
   - Work queue scheduling
   - Queue push success/failure

**SPI Source Tests**:
1. **Value Validation**
   - Expected value match → correct status
   - Unexpected value → LQ_ERROR
   - Communication failure → LQ_ERROR

2. **Transaction Management**
   - Correct SPI configuration
   - Multi-byte reads
   - Timeout handling

**Dual-Inverted Tests**:
1. **State Detection**
   - Normal=1, Inverted=0 → output 1, LQ_OK
   - Normal=0, Inverted=1 → output 0, LQ_OK
   - Both high → LQ_ERROR
   - Both low → LQ_ERROR

2. **Debouncing**
   - Glitch rejection
   - Stable state detection
   - Configurable debounce time

### 4. Merge/Voter Tests

**Location**: `tests/merge/`

**Test Cases**:
1. **Voting Algorithms**
   - Median: [100, 105, 200] → 105
   - Average: [100, 200, 300] → 200
   - Min: [300, 100, 200] → 100
   - Max: [300, 100, 200] → 300

2. **Tolerance Checking**
   - Inputs within tolerance → LQ_OK
   - Inputs exceed tolerance → LQ_INCONSISTENT
   - Single outlier detection

3. **Timeout Handling**
   - All inputs fresh → use all
   - One input stale → use cached value
   - All inputs stale → LQ_TIMEOUT

4. **Range Validation**
   - Voted value in range → LQ_OK
   - Voted value out of range → LQ_OUT_OF_RANGE

**Device Tree**:
```dts
/ {
    q_input1: lq-queue@0 {
        compatible = "zephyr,lq-queue";
        capacity = <8>;
    };

    q_input2: lq-queue@1 {
        compatible = "zephyr,lq-queue";
        capacity = <8>;
    };

    q_output: lq-queue@2 {
        compatible = "zephyr,lq-queue";
        capacity = <16>;
    };

    test_voter: lq-merge@0 {
        compatible = "zephyr,lq-merge-voter";
        input-queues = <&q_input1 &q_input2>;
        output-queue = <&q_output>;
        voting-method = "median";
        tolerance = <50>;
    };
};
```

### 5. Integration Tests

**Location**: `tests/integration/`

**Test Scenarios**:

1. **End-to-End Pipeline**
   - ADC → Queue → Merge → Queue → Application
   - Verify data integrity
   - Verify status propagation
   - Measure latency

2. **Multi-Source Redundancy**
   - 2 ADC sources
   - Median voting
   - Inject fault in one source
   - Verify output remains valid

3. **Range Violation Handling**
   - Normal operation
   - Inject out-of-range value
   - Verify status update
   - Verify callback invocation

4. **Queue Overflow Scenarios**
   - Slow consumer, fast producer
   - Verify drop-oldest behavior
   - Verify statistics update
   - No memory corruption

### 6. Hardware-in-the-Loop Tests

**Location**: `tests/hil/`

**Requirements**:
- Real hardware (ADC, SPI, GPIO)
- Known sensor inputs
- Signal generators

**Test Cases**:
1. **ADC Accuracy**
   - Apply known voltage
   - Verify read value ±accuracy
   - Verify averaging reduces noise

2. **SPI Communication**
   - Read from real SPI device
   - Verify protocol compliance
   - Verify error handling

3. **GPIO Dual-Inverted**
   - Toggle complementary signals
   - Inject both-high fault
   - Inject both-low fault
   - Verify error detection

### 7. Safety Validation Tests

**Location**: `tests/safety/`

**Test Cases**:
1. **Fault Injection**
   - ADC read failure
   - SPI timeout
   - Queue full condition
   - Memory corruption detection

2. **Stress Testing**
   - Maximum polling rate
   - Queue at full capacity
   - Multiple simultaneous errors
   - Thread starvation scenarios

3. **Determinism Verification**
   - Measure maximum latency
   - Verify no dynamic allocation
   - Verify bounded execution time
   - WCET analysis

4. **Error Propagation**
   - Error at source → correct status in queue
   - Timeout in merge → correct status output
   - Multiple error types simultaneously

## Test Matrix

| Component | Unit | Queue | Source | Merge | Integration | HIL | Safety |
|-----------|------|-------|--------|-------|-------------|-----|--------|
| lq_util | ✓ | - | - | - | - | - | - |
| lq_queue | - | ✓ | - | - | ✓ | - | ✓ |
| lq_adc_source | - | - | ✓ | - | ✓ | ✓ | ✓ |
| lq_spi_source | - | - | ✓ | - | ✓ | ✓ | ✓ |
| lq_merge_voter | ✓ | - | - | ✓ | ✓ | - | ✓ |
| lq_dual_inverted | - | - | ✓ | - | - | ✓ | ✓ |

## Continuous Integration

```yaml
# .github/workflows/test.yml
name: Test Layered Queue Driver

on: [push, pull_request]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup Zephyr
        run: west init && west update
      - name: Run Unit Tests
        run: |
          west build -b native_posix tests/util
          west build -t run

  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup Zephyr
        run: west init && west update
      - name: Run Integration Tests
        run: |
          west build -b native_posix tests/integration
          west build -t run
```

## Code Coverage Target

- **Overall**: >90%
- **Critical paths** (voting, validation): 100%
- **Error handling**: >95%

## Acceptance Criteria

1. All unit tests pass
2. All integration tests pass
3. No memory leaks (valgrind on native_posix)
4. No race conditions (thread sanitizer)
5. Code coverage >90%
6. Static analysis clean (sparse, cppcheck)
7. Device tree bindings validate correctly
8. Documentation complete and accurate

## Test Execution

### Quick Test

```bash
./scripts/run_tests.sh quick
```

### Full Test Suite

```bash
./scripts/run_tests.sh full
```

### Coverage Report

```bash
./scripts/run_tests.sh coverage
```

### Hardware Tests

```bash
./scripts/run_tests.sh hil --board=custom_board
```

## Known Limitations

1. Device tree range parsing requires manual array construction (TODO)
2. SPI source needs SPI transaction implementation (TODO)
3. ADC source needs ADC read implementation (TODO)
4. Dual-inverted source not yet implemented (TODO)

## Future Test Improvements

- [ ] Fuzzing for device tree parsing
- [ ] Model-based testing for state machines
- [ ] Performance benchmarking suite
- [ ] Power consumption testing
- [ ] Formal verification of critical algorithms
