# CI/CD HIL Rig Setup

## Overview

Automated hardware testing in CI/CD using a Linux server with USB-connected development boards.

```
┌─────────────────────────────────────────────────────┐
│                  CI Server (Linux)                  │
│                                                     │
│  ┌──────────────┐      ┌──────────────┐            │
│  │ HIL Test     │ UART │              │            │
│  │ Runner       │─────→│  USB Hub     │            │
│  └──────────────┘      └──────────────┘            │
│                             │   │   │              │
└─────────────────────────────┼───┼───┼──────────────┘
                              ↓   ↓   ↓
                    ┌──────┐ ┌──────┐ ┌──────┐
                    │STM32 │ │ESP32 │ │nRF52 │
                    │ F4   │ │DevKit│ │ DK   │
                    └──────┘ └──────┘ └──────┘
```

## Hardware Setup

### Required Components

1. **CI Server**: Linux machine (VM or physical)
   - Ubuntu 22.04 LTS recommended
   - 4GB RAM minimum
   - USB 3.0 ports

2. **Development Boards** (one or more):
   - STM32F4 Discovery
   - ESP32 DevKit
   - nRF52840 DK
   - Any board supported by `platform_adaptors.py`

3. **USB Hub**: Powered USB 3.0 hub (7-10 ports)

4. **USB Cables**: One per board

### Physical Connection

1. Connect all dev boards to USB hub
2. Connect USB hub to CI server
3. Verify persistent device IDs:
```bash
ls -l /dev/serial/by-id/
# Should show:
# usb-STMicroelectronics_STM32_*
# usb-Silicon_Labs_CP2102_*  (ESP32)
# usb-SEGGER_J-Link_*         (nRF52)
```

### Software Dependencies

```bash
# Install build tools
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake python3 python3-pip \
    openocd esptool nrf-command-line-tools

# Install udev rules for non-root access
sudo usermod -a -G dialout $USER
sudo usermod -a -G plugdev $USER

# Add persistent device permissions
sudo tee /etc/udev/rules.d/99-hil-boards.rules << EOF
# STM32
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", MODE="0666"
# ESP32
SUBSYSTEM=="usb", ATTRS{idVendor}=="10c4", MODE="0666"
# nRF52
SUBSYSTEM=="usb", ATTRS{idVendor}=="1366", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger
```

## CI Configuration

### GitHub Actions

```yaml
name: Hardware HIL Tests

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  hardware-hil:
    runs-on: self-hosted  # Requires self-hosted runner with boards
    
    steps:
      - uses: actions/checkout@v3
      
      - name: Build and test on hardware
        run: |
          mkdir build && cd build
          cmake ..
          make
          
      - name: Run HIL tests on all boards
        run: ./tests/hil/ci_hil_runner.sh stm32f4 esp32 nrf52
        
      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v3
        with:
          name: hil-test-results
          path: build/hil/*.tap
```

### GitLab CI

```yaml
hardware-hil:
  stage: test
  tags:
    - hil-rig  # Runner with hardware boards
  
  script:
    - mkdir build && cd build
    - cmake ..
    - make
    - ../tests/hil/ci_hil_runner.sh stm32f4 esp32 nrf52
  
  artifacts:
    when: always
    reports:
      junit: build/hil/*.xml
    paths:
      - build/hil/*.tap
```

### Jenkins

```groovy
pipeline {
    agent { label 'hil-rig' }
    
    stages {
        stage('Build') {
            steps {
                sh '''
                    mkdir -p build && cd build
                    cmake ..
                    make -j$(nproc)
                '''
            }
        }
        
        stage('Hardware HIL Tests') {
            steps {
                sh './tests/hil/ci_hil_runner.sh stm32f4 esp32 nrf52'
            }
        }
    }
    
    post {
        always {
            archiveArtifacts artifacts: 'build/hil/*.tap', allowEmptyArchive: true
            junit 'build/hil/*.xml'
        }
    }
}
```

## Running Tests

### Test All Connected Boards

```bash
./tests/hil/ci_hil_runner.sh
```

Output:
```
===================================================================
CI Hardware-in-Loop Test Runner
===================================================================

Testing boards: stm32f4 esp32 nrf52

-------------------------------------------------------------------
Testing: stm32f4
-------------------------------------------------------------------
Device: /dev/serial/by-id/usb-STMicroelectronics_STM32_...
Transport: uart
Baudrate: 115200

Building firmware for stm32f4...
✓ Build successful
Flashing firmware to stm32f4...
✓ Flash successful
Running HIL tests...
TAP version 14
1..23
ok 1 - hil-test-all-inputs-nominal
ok 2 - hil-test-voting-merge
...
ok 23 - hil-test-latency

✅ stm32f4: All tests passed (23/23)

-------------------------------------------------------------------
Testing: esp32
...

===================================================================
CI HIL Test Summary
===================================================================
✅ stm32f4 - Passed (23 tests)
✅ esp32 - Passed (23 tests)
✅ nrf52 - Passed (23 tests)

Total: 3 passed, 0 failed

🎉 All boards passed!
```

### Test Specific Board

```bash
./tests/hil/ci_hil_runner.sh stm32f4
```

### Manual HIL Test

```bash
# Flash board manually
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg \
    -c "program build/firmware.elf verify reset exit"

# Run tests
./build/hil_test_runner \
    --transport=uart \
    --device=/dev/ttyUSB0 \
    --baudrate=115200
```

## Troubleshooting

### Board Not Detected

```bash
# Check USB devices
lsusb

# Check serial ports
ls -l /dev/serial/by-id/

# Check permissions
groups  # Should include dialout and plugdev
```

### Flash Failures

```bash
# STM32: Check ST-Link connection
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg

# ESP32: Check USB driver
esptool.py chip_id

# nRF52: Check J-Link
nrfjprog --version
```

### Test Timeouts

- Increase timeout in test DTS: `timeout-ms = <5000>;`
- Check UART baudrate matches board configuration
- Verify board is running (LED blinking)

## Cost-Effective Setup

### Minimal Setup (~$100)
- Raspberry Pi 4 (4GB): $55
- STM32F4 Discovery: $25
- USB cables: $10

### Professional Setup (~$500)
- Refurbished PC or NUC: $200
- STM32F4 Discovery: $25
- ESP32 DevKit: $15
- nRF52840 DK: $40
- 10-port USB hub: $30
- USB cables: $20

### Enterprise Setup (~$2000)
- Dedicated server: $800
- 5x different boards: $300
- Managed USB hub: $200
- Power supplies: $100
- UPS backup: $300

## Benefits

✅ **Catch hardware-specific bugs** before deployment
✅ **Test on real peripherals** (ADC, CAN, SPI)
✅ **Validate timing** on actual MCU
✅ **Regression testing** on every commit
✅ **Multi-platform validation** in parallel
✅ **No manual intervention** required
✅ **Same tests** as software simulation

## Best Practices

1. **Use persistent device IDs** (`/dev/serial/by-id/`) not `/dev/ttyUSB0`
2. **Power cycle boards** between test runs
3. **Monitor board temperatures** in long runs
4. **Keep boards firmware-flashable** (don't brick them!)
5. **Version control test results** (TAP files)
6. **Alert on repeated failures** (hardware issues)
7. **Regular board health checks** (flash blank firmware)
