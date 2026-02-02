# Platform-Specific Generators

Each platform has its own generator class that implements platform-specific ISR wrappers, peripheral initialization, and hardware configuration.

## Architecture

All platform generators inherit from `PlatformGenerator` base class and implement:

```python
class PlatformGenerator(ABC):
    def generate_platform_header(self) -> str:
        """Platform-specific headers (HAL includes, etc.)"""
        
    def generate_isr_wrappers(self, nodes) -> str:
        """Hardware interrupt handlers"""
        
    def generate_peripheral_init(self, nodes) -> str:
        """Peripheral initialization code"""
        
    def generate_main(self, nodes) -> str:
        """Main entry point (optional override)"""
```

## Supported Platforms

| Platform | Status | Features |
|----------|--------|----------|
| **baremetal** | ✅ Complete | Minimal stub for custom embedded platforms |
| **stm32** | ✅ Complete | STM32 HAL with ADC, SPI, CAN, GPIO, I2C, UART |
| **zephyr** | ✅ Complete | Zephyr RTOS with ISR-based CAN/UART/ADC/GPIO |
| **esp32** | ⏳ Stub | ESP32 IDF (TODO: full implementation) |
| **samd** | ⏳ Stub | Atmel SAMD ASF4 (TODO: full implementation) |
| **nrf52** | ⏳ Stub | Nordic nRF52 SDK (TODO: full implementation) |
| **avr** | ⏳ Stub | AVR/Arduino (TODO: full implementation) |

## Usage

```python
from generators.platforms import get_platform_generator

# Get platform-specific generator
gen = get_platform_generator('stm32')

# Generate platform code
outputs = gen.generate(nodes, counts)

# Returns:
# {
#   'lq_platform_hw.c': '/* STM32 HAL ISRs and init */',
#   'main.c': '/* STM32 main entry point */'
# }
```

## STM32 Platform (Complete Example)

### Generated ISR Wrappers

```c
/* ADC DMA Conversion Complete Callback */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
    if (hadc->Instance == ADC1) {
        uint16_t value = HAL_ADC_GetValue(&hadc1);
        lq_adc_isr_throttle_adc(value);
    }
}

/* CAN Receive Callback for J1939 PGN */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
    CAN_RxHeaderTypeDef rx_header;
    uint8_t rx_data[8];
    
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK) {
        uint32_t pgn = (rx_header.ExtId >> 8) & 0x3FFFF;
        if (pgn == 0xF004) {  /* Engine Temperature */
            int32_t value = (rx_data[3] << 24) | (rx_data[2] << 16) | 
                            (rx_data[1] << 8) | rx_data[0];
            lq_hw_push(signal_id, value);
        }
    }
}
```

### Generated Peripheral Init

```c
void lq_platform_peripherals_init(void) {
    /* GPIO Configuration */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    
    /* ADC Configuration */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&adc_buffer, 1);
    
    /* CAN Configuration */
    CAN_FilterTypeDef can_filter;
    /* ... filter setup ... */
    HAL_CAN_ConfigFilter(&hcan1, &can_filter);
    HAL_CAN_Start(&hcan1);
    HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);
}
```

### Main Entry Point

```c
int main(void) {
    printf("Layered Queue Driver - STM32 HAL\n");
    
    /* Platform-specific initialization */
    lq_platform_peripherals_init();
    
    /* Initialize layered queue engine */
    if (lq_generated_init() != 0) {
        return 1;
    }
    
    /* Main event loop */
    while (1) {
        lq_generated_dispatch_outputs();
        __WFI();  /* Power-efficient sleep */
    }
}
```

## Adding a New Platform

1. Create new file in `generators/platforms/myplatform.py`:

```python
from .base import PlatformGenerator

class MyPlatformGenerator(PlatformGenerator):
    def __init__(self):
        super().__init__("My Platform")
    
    def generate_platform_header(self) -> str:
        return """#include "myplatform_hal.h"
"""
    
    def generate_isr_wrappers(self, nodes) -> str:
        code = ""
        for node in nodes:
            if node.compatible.startswith('lq,hw-'):
                # Generate platform-specific ISR
                code += f"/* ISR for {node.label} */\n"
        return code
    
    def generate_peripheral_init(self, nodes) -> str:
        return """void lq_platform_peripherals_init(void) {
    /* Initialize peripherals */
}
"""
```

2. Register in `generators/platforms/__init__.py`:

```python
from .myplatform import MyPlatformGenerator

PLATFORM_GENERATORS = {
    'myplatform': MyPlatformGenerator,
    # ... existing platforms
}
```

3. Use it:

```bash
python3 scripts/dts_gen_refactored.py app.dts src/ --platform=myplatform
```

## Platform-Specific Notes

### STM32 HAL
- Assumes CubeMX-generated project with HAL configured
- Generates callbacks for HAL interrupt handlers
- Supports bxCAN and FDCAN variants
- Power-efficient with `__WFI()` in main loop

### Baremetal
- Minimal stub for custom platforms
- User implements hardware-specific code
- Useful for RTOS integration or custom embedded OSes

### ESP32 IDF
- TODO: Use ESP-IDF driver APIs
- FreeRTOS integration
- ESP32-specific peripherals (ADC, WiFi, BLE)

### SAMD ASF4
- TODO: Use Atmel START / ASF4 APIs
- SERCOM peripheral handling
- Event system integration

### nRF52 SDK
- TODO: Use nRF52 SDK drivers
- SAADC, SPIM, UARTE peripherals
- SoftDevice integration for BLE

### AVR
- TODO: Direct register manipulation
- Timer interrupts
- ADC free-running mode
- Arduino compatibility layer

## Testing

Test platform generation with various DTS files:

```bash
# STM32 platform
python3 scripts/dts_gen_refactored.py samples/fault-monitor-example.dts \
    /tmp/test_stm32 --platform=stm32

# Baremetal platform
python3 scripts/dts_gen_refactored.py samples/fault-monitor-example.dts \
    /tmp/test_baremetal --platform=baremetal

# Check generated files
ls -l /tmp/test_stm32/
cat /tmp/test_stm32/lq_platform_hw.c
cat /tmp/test_stm32/main.c
```

## Migration from platform_adaptors.py

The legacy `platform_adaptors.py` (879 lines) has been refactored into:
- `platforms/base.py` - Base class (~150 lines)
- `platforms/stm32.py` - STM32 implementation (~200 lines)
- `platforms/baremetal.py` - Minimal stub (~50 lines)
- Other platforms as stubs (~30 lines each)

**Benefits:**
- Clean separation of concerns
- Each platform is independently testable
- Easy to add new platforms
- I/O-free (returns strings, orchestrator writes files)

## Zephyr Platform (Complete)

### Design Philosophy

The Zephyr generator uses a **deterministic polling strategy**:

**ISR-based (async serial inputs):**
- **CAN**: Direct ISR callbacks (no work queue)
- **UART**: Interrupt-based RX callbacks

**Polled by engine (deterministic inputs):**
- **ADC**: Sampled at fixed intervals by `lq_engine_step()`
- **GPIO**: Read at fixed intervals by `lq_engine_step()`
- **SPI**: Read at fixed intervals by `lq_engine_step()`

This hybrid approach provides:
- **Lower latency** for async data (CAN/UART frames buffered immediately)
- **Determinism** for periodic data (ADC/GPIO/SPI sampled at fixed rate)
- **Minimal interrupt load** (only 2 ISRs instead of 5+)
- **Predictable timing** for control loops

### CAN/UART: ISR-based (No Work Queue)

**Traditional approach** (work queue):
```c
static struct k_msgq can_rx_msgq;
can_add_rx_filter_msgq(dev, &can_rx_msgq, &filter);  // Uses message queue
```

**Generated approach** (direct ISR):
```c
static void lq_can_rx_callback(const struct device *dev,
                                struct can_frame *frame,
                                void *user_data) {
    lq_hw_can_push(signal_id, frame->data, frame->dlc, timestamp);
}
can_add_rx_filter(dev, lq_can_rx_callback, NULL, &filter);  // Direct ISR
```

### ADC/GPIO/SPI: Polled by Engine

**Generated polling functions** (called by `lq_engine_step()`):
```c
/* Called deterministically every cycle */
void lq_platform_poll_adc_throttle_adc(void) {
    adc_read(ADC0_DEV, &adc_seq_throttle_adc);
    lq_hw_push(signal_id, value, timestamp);
}

void lq_platform_poll_gpio_door_sensor(void) {
    int value = gpio_pin_get(gpio_dev, pin);
    lq_hw_push(signal_id, value, timestamp);
}
```

### Features

- **CAN**: Direct ISR callbacks, J1939 PGN filtering
- **UART**: Interrupt RX callbacks
- **ADC**: Deterministic polling by engine
- **GPIO**: Deterministic polling by engine  
- **SPI**: Deterministic polling by engine
- Devicetree integration
- Zephyr native driver APIs

