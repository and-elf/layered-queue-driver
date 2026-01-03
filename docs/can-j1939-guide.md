# CAN and J1939 Support

The Layered Queue Driver provides full CAN bus support with J1939 protocol implementation for automotive and heavy-duty vehicle applications.

## Features

### CAN Input
- **Receive J1939 messages** from other ECUs on the CAN bus
- **Filter by PGN** (Parameter Group Number)
- **Extract data** from standard J1939 formats
- **Stale detection** for lost CAN messages

### CAN Output
- **Transmit J1939 PGNs** at configurable rates
- **Priority-based transmission** (0-7, where 0 is highest)
- **Proper J1939 identifier formatting** (29-bit extended CAN)
- **Multiple output types**: EEC1, EEC2, ET1, EFL/P1, etc.

### J1939 Diagnostics
- **DM1 (Active DTCs)**: Broadcast active diagnostic trouble codes
- **DM0 (Lamp Status)**: Broadcast stop/warning lamp states
- **Automatic DTC generation** from error conditions
- **Lamp control**: MIL, Red Stop, Amber Warning, Protect
- **SPN/FMI mapping** for standard J1939 fault codes

## Quick Start

### 1. Define CAN Input in DTS

```dts
rpm_can: rpm-from-other-ecu {
    compatible = "lq,hw-can-input";
    label = "rpm_can";
    signal_id = <10>;
    pgn = <65265>;      /* J1939 EEC1 */
    stale_us = <200000>; /* 200ms timeout */
    hw_instance = <1>;   /* CAN1 */
};
```

### 2. Define CAN Output

```dts
eec1_output: engine-controller-1 {
    compatible = "lq,cyclic-output";
    source_signal_id = <20>;
    output_type = "j1939";
    target_id = <65265>;    /* PGN 0xFEF1 - EEC1 */
    period_us = <100000>;   /* 10Hz */
    priority = <3>;
    source_address = <0x28>; /* Engine #1 */
    hw_instance = <1>;
};
```

### 3. Add Error Detection with DM1

```dts
coolant_overtemp: coolant-error {
    compatible = "lq,error-detector";
    input_signal_id = <3>;
    error_signal_id = <31>;
    error_conditions = "value > 110000"; /* 110°C */
    error_spn = <110>;  /* J1939 SPN for coolant temp */
    error_fmi = <0>;    /* Data above normal */
};

dm1_output: active-dtcs {
    compatible = "lq,j1939-dm1-output";
    error_signal_ids = <30 31 32>;
    pgn = <65226>;      /* J1939 DM1 */
    priority = <6>;
    period_us = <1000000>; /* 1Hz */
    malfunction_lamp = "on_error";
    red_stop_lamp = "on_error_32";
};
```

### 4. Generate Platform-Specific Code

```bash
# For STM32 with CAN peripheral
python3 scripts/dts_gen.py automotive_can.dts src/ --platform=stm32

# For ESP32 with TWAI (CAN)
python3 scripts/dts_gen.py automotive_can.dts main/ --platform=esp32

# For nRF52 with external MCP2515 CAN controller
python3 scripts/dts_gen.py automotive_can.dts src/ --platform=nrf52
```

## J1939 Standard PGNs

### Common Parameter Group Numbers

| PGN | Name | Description | Rate | Priority |
|-----|------|-------------|------|----------|
| 65265 | EEC1 | Electronic Engine Controller 1 (RPM, torque) | 10Hz | 3 |
| 65266 | EEC2 | Electronic Engine Controller 2 (fuel, accel) | 10Hz | 3 |
| 65262 | ET1 | Engine Temperature 1 (coolant, oil) | 1Hz | 6 |
| 65263 | EFL/P1 | Engine Fluid Level/Pressure 1 | 1Hz | 6 |
| 65226 | DM1 | Active Diagnostic Trouble Codes | 1Hz | 6 |
| 65227 | DM2 | Previously Active DTCs | On request | 6 |
| 59904 | REQUEST | Request for PGN | Event | 6 |
| 61444 | EEC1 | Electronic Engine Controller 1 | 10Hz | 3 |

### Common Suspect Parameter Numbers (SPNs)

| SPN | Parameter | Unit | Range |
|-----|-----------|------|-------|
| 190 | Engine Speed | RPM | 0-8,031.875 |
| 110 | Engine Coolant Temperature | °C | -40 to 210 |
| 100 | Engine Oil Pressure | kPa | 0-1000 |
| 175 | Engine Oil Temperature | °C | -273 to 1735 |
| 183 | Fuel Rate | L/h | 0-3212.75 |
| 84 | Vehicle Speed | km/h | 0-250.996 |
| 91 | Accelerator Pedal Position | % | 0-100 |

### Failure Mode Indicators (FMI)

| FMI | Description |
|-----|-------------|
| 0 | Data valid but above normal operational range |
| 1 | Data valid but below normal operational range |
| 2 | Data erratic, intermittent, or incorrect |
| 3 | Voltage above normal |
| 4 | Voltage below normal |
| 5 | Current below normal |
| 6 | Current above normal |
| 7 | Mechanical system not responding properly |
| 11 | Root cause not known |
| 12 | Bad intelligent device or component |
| 13 | Out of calibration |
| 31 | Condition exists |

## Platform-Specific Implementation

### STM32 HAL

**Generated ISR:**
```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan->Instance == CAN1) {
        CAN_RxHeaderTypeDef rx_header;
        uint8_t rx_data[8];
        
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
            uint32_t id = rx_header.ExtId;
            uint32_t pgn = (id >> 8) & 0x3FFFF;
            
            if (pgn == 65265) {  /* EEC1 */
                int32_t rpm = (rx_data[3] << 24) | (rx_data[2] << 16) |
                              (rx_data[1] << 8) | rx_data[0];
                lq_hw_push(10, rpm);
            }
        }
    }
}
```

**CubeMX Configuration:**
1. Enable CAN1 peripheral
2. Set bit rate to 250 kbps (J1939 standard)
3. Configure as Extended ID (29-bit)
4. Enable RX FIFO0 interrupt
5. Configure CAN filter for desired PGNs

**Output (Transmit):**
```c
void lq_can_transmit_j1939(uint32_t pgn, uint8_t priority, uint8_t sa, 
                           const uint8_t *data, uint8_t len)
{
    CAN_TxHeaderTypeDef tx_header;
    uint32_t mailbox;
    
    /* Build J1939 29-bit identifier */
    tx_header.ExtId = lq_j1939_build_id_from_pgn(pgn, priority, sa);
    tx_header.IDE = CAN_ID_EXT;
    tx_header.RTR = CAN_RTR_DATA;
    tx_header.DLC = len;
    
    HAL_CAN_AddTxMessage(&hcan1, &tx_header, (uint8_t*)data, &mailbox);
}
```

### ESP32 IDF (TWAI)

**Generated ISR:**
```c
void lq_can_receive_rpm_can(void)
{
    twai_message_t rx_msg;
    
    if (twai_receive(&rx_msg, pdMS_TO_TICKS(0)) == ESP_OK) {
        if (rx_msg.extd && !rx_msg.rtr) {
            uint32_t pgn = (rx_msg.identifier >> 8) & 0x3FFFF;
            
            if (pgn == 65265) {  /* EEC1 */
                int32_t value = (rx_msg.data[3] << 24) | (rx_msg.data[2] << 16) |
                                (rx_msg.data[1] << 8) | rx_msg.data[0];
                lq_hw_push(10, value);
            }
        }
    }
}
```

**Configuration:**
```c
void lq_platform_peripherals_init(void)
{
    /* TWAI (CAN) Configuration for J1939 */
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        GPIO_NUM_21,  /* TX */
        GPIO_NUM_22,  /* RX */
        TWAI_MODE_NORMAL
    );
    
    /* J1939 uses 250 kbps */
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    
    /* Accept all extended IDs */
    twai_filter_config_t f_config = {
        .acceptance_code = 0,
        .acceptance_mask = 0,
        .single_filter = true
    };
    
    twai_driver_install(&g_config, &t_config, &f_config);
    twai_start();
}
```

### Nordic nRF52 (External MCP2515)

Since nRF52 doesn't have built-in CAN, use external MCP2515 via SPI:

```c
void lq_can_receive_rpm_can(void)
{
    /* Read from MCP2515 via SPI */
    if (nrf_drv_can_read_message(can_rx_buffer, sizeof(can_rx_buffer)) == NRF_SUCCESS) {
        uint32_t id = (can_rx_buffer[0] << 24) | (can_rx_buffer[1] << 16) |
                      (can_rx_buffer[2] << 8) | can_rx_buffer[3];
        uint32_t pgn = (id >> 8) & 0x3FFFF;
        
        if (pgn == 65265) {
            int32_t value = (can_rx_buffer[8] << 24) | (can_rx_buffer[7] << 16) |
                            (can_rx_buffer[6] << 8) | can_rx_buffer[5];
            lq_hw_push(10, value);
        }
    }
}
```

## J1939 DM1 Implementation

### DTC Structure

A J1939 Diagnostic Trouble Code (DTC) consists of:
- **SPN** (Suspect Parameter Number): 19 bits - identifies the component
- **FMI** (Failure Mode Indicator): 5 bits - identifies the failure type
- **OC** (Occurrence Count): 7 bits - number of times fault occurred

### Creating DTCs

```c
#include "lq_j1939.h"

/* Create DTC: Coolant temp sensor above normal */
uint32_t dtc = lq_j1939_create_dtc(
    110,  /* SPN: Engine Coolant Temperature */
    0,    /* FMI: Data above normal */
    1     /* OC: First occurrence */
);

/* Add to DM1 message */
lq_j1939_dm1_t dm1 = {
    .malfunction_lamp = J1939_LAMP_ON,
    .amber_warning_lamp = J1939_LAMP_ON,
    .dtc_list[0] = dtc,
    .dtc_count = 1
};

/* Format into CAN frame */
uint8_t can_data[8];
lq_j1939_format_dm1(&dm1, can_data, sizeof(can_data));

/* Transmit on CAN bus */
lq_can_transmit_j1939(J1939_PGN_DM1, 6, 0x28, can_data, 8);
```

### Automatic DM1 Generation

The DTS compiler automatically generates DM1 messages when error conditions are detected:

```dts
/* Error detector */
oil_low: oil-pressure-error {
    error_spn = <100>;    /* Oil pressure SPN */
    error_fmi = <1>;      /* Below normal */
};

/* DM1 automatically includes this DTC when error_signal is active */
```

## Integration Examples

### Complete Automotive ECU

See [samples/j1939/automotive_can_system.dts](../samples/j1939/automotive_can_system.dts) for a complete example with:
- Multiple CAN inputs from other ECUs
- Triple-redundant sensor voting
- Automatic fault detection
- DM1 diagnostic message generation
- Multiple J1939 PGN outputs (EEC1, EEC2, ET1, EFL/P1)

### Testing on Hardware

1. **STM32 Discovery + CAN Transceiver**
   ```bash
   python3 scripts/dts_gen.py automotive_can.dts Src/ --platform=stm32
   # Flash to STM32F4 Discovery board
   # Connect TJA1050 CAN transceiver to CAN1 pins (PA11/PA12)
   ```

2. **ESP32 DevKit + SN65HVD230**
   ```bash
   python3 scripts/dts_gen.py automotive_can.dts main/ --platform=esp32
   idf.py build flash
   # Connect SN65HVD230 to GPIO21 (TX) and GPIO22 (RX)
   ```

3. **Test with CAN Analyzer**
   - Use PCAN-USB, Kvaser, or similar
   - Set to 250 kbps
   - Enable 29-bit extended IDs
   - Monitor for J1939 PGNs

## Best Practices

1. **Always use 250 kbps** for J1939 networks
2. **Set proper priorities**: 3 for control messages, 6 for monitoring
3. **Implement timeout detection** for critical CAN inputs
4. **Use source addresses** according to J1939 spec (0x00-0xFD)
5. **Test with real J1939 tools** before deployment
6. **Implement DM1 for all critical faults**
7. **Follow J1939-71 and J1939-73** for diagnostic protocols

## Troubleshooting

### CAN Messages Not Received
- ✅ Check CAN bus termination (120Ω on both ends)
- ✅ Verify bit rate (250 kbps for J1939)
- ✅ Check transceiver power and connections
- ✅ Verify extended ID (29-bit) mode enabled
- ✅ Review CAN filter configuration

### DM1 Not Transmitting
- ✅ Verify error condition is actually triggered
- ✅ Check error_signal_id mapping
- ✅ Confirm SPN/FMI are valid J1939 codes
- ✅ Verify CAN transmit buffer is not full

### Wrong Data in Messages
- ✅ Check byte order (J1939 uses little-endian)
- ✅ Verify scaling factors for SPNs
- ✅ Confirm PGN filtering is correct
- ✅ Check that SA (source address) matches expected ECU

## References

- [J1939 Standards (SAE)](https://www.sae.org/standards/content/j1939/)
- [J1939-71: Vehicle Application Layer](https://www.sae.org/standards/content/j1939/71/)
- [J1939-73: Diagnostics Layer](https://www.sae.org/standards/content/j1939/73/)
- [STM32 CAN Peripheral](https://www.st.com/resource/en/application_note/dm00105974.pdf)
- [ESP32 TWAI Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html)
