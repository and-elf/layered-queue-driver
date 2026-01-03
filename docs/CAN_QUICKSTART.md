# CAN and J1939 Quick Start

## What Was Added

✅ **Full CAN support** for receiving and transmitting J1939 messages  
✅ **J1939 protocol implementation** with DM0/DM1 diagnostics  
✅ **Platform-specific CAN drivers** for STM32, ESP32, and nRF52  
✅ **Automatic fault detection** with DTC generation  
✅ **Real ISRs** ready for production automotive/heavy-duty applications

## Try It Out

### 1. Generate Code for Your Platform

```bash
# STM32 with built-in CAN
python3 scripts/dts_gen.py samples/j1939/automotive_can_system.dts src/ --platform=stm32

# ESP32 with TWAI (built-in CAN)
python3 scripts/dts_gen.py samples/j1939/automotive_can_system.dts main/ --platform=esp32

# nRF52 with external MCP2515
python3 scripts/dts_gen.py samples/j1939/automotive_can_system.dts src/ --platform=nrf52
```

### 2. What Gets Generated

- **lq_generated.h/c**: Core engine setup (platform-agnostic)
- **lq_platform_hw.c**: Real CAN ISRs with J1939 support
  - `HAL_CAN_RxFifo0MsgPendingCallback()` (STM32)
  - `lq_can_receive_*()` (ESP32/nRF52)
  - Automatic PGN filtering
  - J1939 identifier parsing
  - CAN filter configuration

### 3. Key Features

**CAN Input (Receive from other ECUs):**
```dts
rpm_can: rpm-from-transmission {
    compatible = "lq,hw-can-input";
    signal_id = <10>;
    pgn = <65265>;      /* J1939 EEC1 */
    stale_us = <200000>; /* 200ms timeout */
    hw_instance = <1>;   /* CAN1 */
};
```

**CAN Output (Transmit to bus):**
```dts
eec1_output: engine-data {
    compatible = "lq,cyclic-output";
    source_signal_id = <20>;
    output_type = "j1939";
    target_id = <65265>;    /* PGN 0xFEF1 */
    period_us = <100000>;   /* 10Hz */
    priority = <3>;
    source_address = <0x28>;
};
```

**J1939 DM1 Diagnostics:**
```dts
dm1_output: fault-broadcast {
    compatible = "lq,j1939-dm1-output";
    error_signal_ids = <30 31 32>;
    pgn = <65226>;      /* DM1 */
    malfunction_lamp = "on_error";
    red_stop_lamp = "on_error_32";  /* Critical oil pressure */
};
```

## Hardware Setup

### STM32 + CAN Transceiver (TJA1050/MCP2551)

**Important:** STM32 has a **built-in CAN controller** (bxCAN on F1/F4, FDCAN on newer). You only need an external CAN transceiver chip to convert logic levels to differential CAN bus signals.

```
STM32 (Built-in CAN) | CAN Transceiver Chip | J1939 Bus
---------------------|----------------------|----------
PA11 (CAN1_RX)       | RX                   |
PA12 (CAN1_TX)       | TX                   |
3.3V/5V              | VCC                  |
GND                  | GND                  | Pin 3
                     | CANH                 | Pin 6 (120Ω termination)
                     | CANL                 | Pin 14 (120Ω termination)
```

**Generated code uses:**
- ✅ STM32's built-in CAN1 peripheral (via HAL)
- ✅ Hardware CAN message filters
- ✅ DMA-capable CAN interrupts
- ✅ No external CAN controller needed!

**Bit Rate:** 250 kbps (J1939 standard)  
**ID Type:** Extended 29-bit  
**Voltage:** 5V CAN transceiver (3.3V works too)

### ESP32 + SN65HVD230 Transceiver
```
ESP32 Pin   | SN65HVD230 | J1939 Bus
------------|------------|----------
GPIO21 (TX) | TX         |
GPIO22 (RX) | RX         |
3.3V        | VCC        |
GND         | GND        | Pin 3
            | CANH       | Pin 6
            | CANL       | Pin 14
```

## J1939 Standard PGNs

| PGN   | Name  | Description           | Rate | Data                    |
|-------|-------|-----------------------|------|-------------------------|
| 65265 | EEC1  | Engine Controller 1   | 10Hz | RPM, torque             |
| 65266 | EEC2  | Engine Controller 2   | 10Hz | Fuel rate, accel pedal  |
| 65262 | ET1   | Engine Temperature 1  | 1Hz  | Coolant, oil temp       |
| 65263 | EFL/P1| Fluid Level/Pressure  | 1Hz  | Oil pressure, fuel level|
| 65226 | DM1   | Active DTCs           | 1Hz  | Fault codes, lamps      |

## Example Application

The complete example in `samples/j1939/automotive_can_system.dts` demonstrates:

- ✅ **Triple-redundant RPM sensing** (2x ADC + 1x CAN)
- ✅ **Median voting** for sensor fusion
- ✅ **CAN inputs** from transmission, ABS, injection ECUs
- ✅ **Error detection** (high temp, low oil pressure)
- ✅ **Automatic DM1** transmission on faults
- ✅ **Lamp control** (MIL, Red Stop, Amber Warning)
- ✅ **Multiple J1939 outputs** (EEC1, EEC2, ET1, EFL/P1)

## Testing

### With CAN Analyzer
```bash
# Monitor CAN bus at 250 kbps, extended IDs
candump can0

# Should see:
# can0  18FEF128  [8]  00 00 00 00 FF FF FF FF  # EEC1 (10Hz)
# can0  18FEEE28  [8]  00 00 FF FF FF FF FF FF  # ET1 (1Hz)
# can0  18FECA28  [8]  00 00 6E 00 00 19 00 00  # DM1 (if fault)
```

### Common Issues

**No CAN messages received?**
- ✅ Check 120Ω termination on both ends of bus
- ✅ Verify bit rate is 250 kbps
- ✅ Ensure extended ID (29-bit) mode enabled
- ✅ Check CANH/CANL are not swapped

**DM1 not transmitting?**
- ✅ Verify error condition is actually triggered
- ✅ Check SPN/FMI values are valid
- ✅ Ensure CAN TX buffer is not full

## Documentation

- **Full guide**: [docs/can-j1939-guide.md](docs/can-j1939-guide.md)
- **Platform adaptors**: [docs/platform-adaptors.md](docs/platform-adaptors.md)
- **Quick reference**: [docs/platform-quick-reference.md](docs/platform-quick-reference.md)

## Next Steps

1. Generate code for your platform
2. Configure CAN hardware in CubeMX/platformio
3. Connect CAN transceiver to hardware
4. Flash and test with CAN analyzer
5. Deploy on real J1939 vehicle network!
