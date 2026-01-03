# HIL Test DTS Format

HIL tests are defined in devicetree format for declarative, platform-agnostic testing.

## Test Structure

```dts
/ {
    hil-test-rpm-voting {
        compatible = "lq,hil-test";
        description = "Triple redundant RPM sensor voting";
        timeout-ms = <5000>;
        
        sequence {
            step@0 {
                action = "inject-adc";
                channel = <0>;
                value = <3000>;
                delay-ms = <10>;
            };
            
            step@1 {
                action = "wait-gpio-high";
                pin = <5>;
                timeout-ms = <100>;
            };
            
            step@2 {
                action = "expect-can";
                pgn = <61444>;  /* EEC1 */
                byte@0 {
                    min = <2900>;
                    max = <3100>;
                };
                timeout-ms = <200>;
            };
        };
    };
};
```

## Test Actions

### Input Injection

**inject-adc**: Inject ADC sample
```dts
step@N {
    action = "inject-adc";
    channel = <0>;          /* ADC channel */
    value = <2500>;         /* Raw/scaled value */
    delay-ms = <10>;        /* Optional delay after */
};
```

**inject-spi**: Inject SPI sensor data
```dts
step@N {
    action = "inject-spi";
    channel = <0>;
    data = [0x12 0x34 0x56 0x78];
};
```

**inject-can**: Inject CAN message
```dts
step@N {
    action = "inject-can";
    can-id = <0x18FEF100>;  /* J1939 extended ID */
    extended = <1>;          /* 1 = 29-bit, 0 = 11-bit */
    data = [0xE8 0x5E 0x00 0x00 0x00 0x00 0x00 0x00];
};
```

**inject-can-pgn**: Inject J1939 CAN by PGN (convenience)
```dts
step@N {
    action = "inject-can-pgn";
    pgn = <61444>;          /* EEC1 */
    priority = <3>;
    source-addr = <0x28>;
    data = [0xE8 0x5E 0x00 0x00 0x00 0x00 0x00 0x00];
};
```

### Output Verification

**wait-gpio-high / wait-gpio-low**: Wait for GPIO state
```dts
step@N {
    action = "wait-gpio-high";
    pin = <5>;
    timeout-ms = <100>;
};
```

**expect-can**: Expect CAN message with validation
```dts
step@N {
    action = "expect-can";
    can-id = <0x18FEF100>;  /* Optional: filter by ID */
    pgn = <61444>;          /* Optional: filter by J1939 PGN */
    
    /* Byte validation (optional) */
    byte@0 {
        min = <100>;
        max = <200>;
    };
    byte@1 {
        value = <0x5E>;     /* Exact match */
    };
    
    timeout-ms = <200>;
};
```

**expect-can-count**: Expect N CAN messages
```dts
step@N {
    action = "expect-can-count";
    pgn = <61444>;
    count = <10>;           /* Expect 10 messages */
    period-ms = <100>;      /* Within 1 second */
    tolerance-ms = <10>;    /* ±10ms timing tolerance */
};
```

### Timing and Synchronization

**delay**: Simple delay
```dts
step@N {
    action = "delay";
    duration-ms = <100>;
};
```

**measure-latency**: Measure input-to-output latency
```dts
step@N {
    action = "measure-latency";
    
    trigger {
        action = "inject-adc";
        channel = <0>;
        value = <3000>;
    };
    
    response {
        action = "expect-gpio-high";
        pin = <5>;
    };
    
    max-latency-us = <50000>;  /* 50ms max */
};
```

### Fault Injection

**inject-fault**: Inject error condition
```dts
step@N {
    action = "inject-fault";
    type = "out-of-range";
    channel = <0>;
    value = <9999>;         /* Out of valid range */
};
```

**expect-dm1**: Expect J1939 DM1 diagnostic message
```dts
step@N {
    action = "expect-dm1";
    
    dtc {
        spn = <110>;        /* Coolant temperature */
        fmi = <0>;          /* Above normal */
    };
    
    lamp {
        mil = <1>;          /* MIL lamp on */
        amber = <1>;
    };
    
    timeout-ms = <1500>;
};
```

## Complete Example

```dts
/ {
    /* Test 1: Basic sensor fusion */
    hil-test-sensor-fusion {
        compatible = "lq,hil-test";
        description = "ADC sensor voting with degradation";
        
        sequence {
            /* Inject normal values from both sensors */
            step@0 {
                action = "inject-adc";
                channel = <0>;
                value = <2500>;
            };
            
            step@1 {
                action = "inject-adc";
                channel = <1>;
                value = <2505>;  /* Slight difference */
            };
            
            /* Expect merged output to be in range */
            step@2 {
                action = "expect-can";
                pgn = <61444>;
                byte@0 { min = <2480>; max = <2520>; };
                timeout-ms = <100>;
            };
            
            /* Inject fault on sensor 1 */
            step@3 {
                action = "inject-fault";
                type = "out-of-range";
                channel = <0>;
                value = <9999>;
            };
            
            /* Expect DM1 diagnostic */
            step@4 {
                action = "expect-dm1";
                dtc { spn = <0>; fmi = <2>; };
                timeout-ms = <1500>;
            };
            
            /* Verify system still outputs (degraded mode) */
            step@5 {
                action = "expect-can";
                pgn = <61444>;
                timeout-ms = <100>;
            };
        };
    };
    
    /* Test 2: CAN input processing */
    hil-test-can-input {
        compatible = "lq,hil-test";
        description = "J1939 CAN message reception";
        
        sequence {
            /* Send EEC1 message */
            step@0 {
                action = "inject-can-pgn";
                pgn = <61444>;
                priority = <3>;
                source-addr = <0x00>;
                data = [0xE8 0x5E 0x00 0x00 0x00 0x00 0x00 0x00];
            };
            
            /* Verify processed and retransmitted */
            step@1 {
                action = "expect-can";
                pgn = <61444>;
                timeout-ms = <50>;
            };
        };
    };
    
    /* Test 3: Real-time performance */
    hil-test-latency {
        compatible = "lq,hil-test";
        description = "End-to-end latency measurement";
        
        sequence {
            step@0 {
                action = "measure-latency";
                
                trigger {
                    action = "inject-adc";
                    channel = <0>;
                    value = <3000>;
                };
                
                response {
                    action = "expect-can";
                    pgn = <61444>;
                };
                
                max-latency-us = <10000>;  /* 10ms */
            };
        };
    };
};
```

## Running Tests

```bash
# Start SUT in HIL mode
./automotive_app --hil-sut &
SUT_PID=$!

# Run test
./lq_hil_runner --test hil_test.dts --sut-pid $SUT_PID

# Cleanup
kill $SUT_PID
```

## Test Results

Test runner outputs TAP (Test Anything Protocol) format:

```
TAP version 14
1..3
ok 1 - hil-test-sensor-fusion
ok 2 - hil-test-can-input
ok 3 - hil-test-latency (latency: 8234us)
```
