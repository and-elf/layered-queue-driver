# Layered Queue Example Application

## Overview

This sample demonstrates the layered queue driver architecture with:
- ADC source driver reading pressure sensor
- SPI source driver reading state sensor
- Merge voter combining redundant inputs
- Queue monitoring and statistics

## Building and Running

```bash
west build -b <your_board> samples/basic
west flash
```

## Expected Output

```
[00:00:00.100] <inf> lq_queue: Initialized queue q_pressure (capacity=16, policy=0)
[00:00:00.101] <inf> lq_queue: Initialized queue q_state (capacity=8, policy=1)
[00:00:00.102] <inf> lq_queue: Initialized queue q_merged (capacity=32, policy=0)
[00:00:00.150] <inf> lq_adc_source: ADC source initialized (ch=2, interval=100ms)
[00:00:00.151] <inf> lq_spi_source: SPI source initialized (reg=0x10, interval=50ms)
[00:00:00.200] <inf> lq_merge_voter: Merge voter initialized (2 inputs, method=0)
[00:00:00.250] <inf> lq_example: Layered Queue Example Application
[00:00:00.251] <inf> lq_example: All queues ready
[00:00:00.300] <inf> lq_example: Pressure: 2500 (status=0)
[00:00:00.350] <inf> lq_example: Merged value: 2450 [OK] @ 350 ms
[00:00:05.000] <inf> lq_example: Pressure queue: 8/16 items, 0 dropped, peak 10
[00:00:05.001] <inf> lq_example: Merged queue: 450 written, 450 read
```

## Configuration

Edit the device tree overlay to customize:
- ADC channel and range validation
- SPI device address and expected values
- Queue capacities and drop policies
- Merge voting method and tolerance

## Device Tree Overlay Example

```dts
/ {
    q_pressure {
        capacity = <32>;  // Increase capacity
    };

    adc_pressure {
        poll-interval-ms = <50>;  // Faster polling
        averaging = <8>;          // More averaging
    };
};
```
