# Peripheral Initialization Guide

## Overview

When porting to embedded hardware, you need to initialize peripherals (ADC, SPI, I2C, UART, GPIO, etc.) before the Layered Queue engine can use them. This guide explains how peripheral initialization works and how to integrate it with your project.

## How It Works

### Generated Code

The code generator creates `lq_generated_init()` which calls your peripheral initialization:

```c
// In generated lq_generated.c
int lq_generated_init(void) {
    /* Initialize engine */
    int ret = lq_engine_init(&g_lq_engine);
    if (ret != 0) return ret;
    
    /* Platform-specific peripheral init */
    #ifdef LQ_PLATFORM_INIT
    lq_platform_peripherals_init();  // <-- Calls YOUR code
    #endif
    
    return 0;
}
```

### Your Implementation

You provide `lq_platform_peripherals_init()` in your own source file:

```c
// user_peripherals.c
#include "lq_platform.h"

void lq_platform_peripherals_init(void)
{
    // Initialize your hardware peripherals
    // ADC, SPI, I2C, UART, GPIO, CAN, etc.
}
```

### CMake Integration

Add your peripheral initialization file and enable the feature:

```cmake
add_lq_application(motor_driver
  DTS motor_system.dts
  PLATFORM stm32
  SOURCES user_peripherals.c  # Your peripheral init code
)

# Enable peripheral initialization
target_compile_definitions(motor_driver PRIVATE
  LQ_PLATFORM_INIT=1
)
```

## Platform-Specific Examples

### STM32 with CubeMX

**Recommended approach**: Let CubeMX generate HAL initialization, then call it from your wrapper:

**Step 1**: Configure peripherals in CubeMX
- Enable ADC1 with DMA
- Enable SPI1 with interrupts  
- Enable I2C1
- Enable UART2
- Configure GPIO pins
- Generate code

**Step 2**: Create your wrapper (`src/user_peripherals.c`):

```c
#include "lq_platform.h"
#include "main.h"  // CubeMX-generated

// CubeMX-generated handles
extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi1;
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;

void lq_platform_peripherals_init(void)
{
    // CubeMX already initialized clocks and GPIOs in SystemClock_Config()
    // and MX_GPIO_Init(), called from main()
    
    // Start ADC with DMA
    uint32_t adc_buffer;
    HAL_ADC_Start_DMA(&hadc1, &adc_buffer, 1);
    
    // Start SPI in interrupt mode
    uint8_t spi_rx_buffer[2];
    HAL_SPI_Receive_IT(&hspi1, spi_rx_buffer, 2);
    
    // I2C is ready after MX_I2C1_Init()
    // Nothing more needed
    
    // Start UART reception
    uint8_t uart_rx_byte;
    HAL_UART_Receive_IT(&huart2, &uart_rx_byte, 1);
}
```

**Step 3**: Update your CMakeLists.txt:

```cmake
add_lq_application(stm32_app
  DTS stm32_system.dts
  PLATFORM stm32
  SOURCES 
    src/user_peripherals.c
    # CubeMX-generated files
    Core/Src/main.c
    Core/Src/stm32f4xx_it.c
    Core/Src/stm32f4xx_hal_msp.c
)

target_compile_definitions(stm32_app PRIVATE
  LQ_PLATFORM_INIT=1
  STM32F407xx  # Your MCU
)

target_include_directories(stm32_app PRIVATE
  Core/Inc
  Drivers/STM32F4xx_HAL_Driver/Inc
  Drivers/CMSIS/Device/ST/STM32F4xx/Include
  Drivers/CMSIS/Include
)
```

### ESP32 with ESP-IDF

ESP-IDF provides driver APIs for all peripherals:

```c
#include "lq_platform.h"
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "driver/gpio.h"

static spi_device_handle_t spi;

void lq_platform_peripherals_init(void)
{
    // ADC configuration
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
    
    // GPIO configuration
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_4, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);
    
    // I2C configuration
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM_0, &i2c_conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    
    // SPI configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = 5,
        .queue_size = 1,
    };
    
    spi_bus_initialize(HSPI_HOST, &bus_cfg, 1);
    spi_bus_add_device(HSPI_HOST, &dev_cfg, &spi);
    
    // UART configuration
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_1, 256, 0, 0, NULL, 0);
}
```

### Nordic nRF52 SDK

Nordic SDK uses driver initialization:

```c
#include "lq_platform.h"
#include "nrf_drv_saadc.h"
#include "nrf_drv_spi.h"
#include "nrf_drv_twi.h"
#include "nrf_drv_uart.h"
#include "nrf_gpio.h"

static nrf_saadc_value_t adc_buffer[8];
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);
static const nrf_drv_twi_t twi = NRF_DRV_TWI_INSTANCE(0);

void saadc_callback(nrf_drv_saadc_evt_t const * p_event);
void spi_handler(nrf_drv_spi_evt_t const * p_event, void * p_context);

void lq_platform_peripherals_init(void)
{
    ret_code_t err_code;
    
    // GPIO
    nrf_gpio_cfg_input(4, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_output(2);
    
    // SAADC (ADC)
    nrf_saadc_channel_config_t channel_config =
        NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN0);
    
    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);
    
    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);
    
    err_code = nrf_drv_saadc_buffer_convert(adc_buffer, 1);
    APP_ERROR_CHECK(err_code);
    
    // SPI
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.ss_pin   = 4;
    spi_config.miso_pin = 28;
    spi_config.mosi_pin = 29;
    spi_config.sck_pin  = 3;
    
    err_code = nrf_drv_spi_init(&spi, &spi_config, spi_handler, NULL);
    APP_ERROR_CHECK(err_code);
    
    // TWI (I2C)
    nrf_drv_twi_config_t twi_config = NRF_DRV_TWI_DEFAULT_CONFIG;
    twi_config.scl = 27;
    twi_config.sda = 26;
    twi_config.frequency = NRF_DRV_TWI_FREQ_100K;
    
    err_code = nrf_drv_twi_init(&twi, &twi_config, NULL, NULL);
    APP_ERROR_CHECK(err_code);
    
    nrf_drv_twi_enable(&twi);
}
```

### Bare Metal / Direct Register Access

For maximum control or minimal overhead:

```c
#include "lq_platform.h"
#include "stm32f4xx.h"  // Or your MCU's register definitions

void lq_platform_peripherals_init(void)
{
    // Enable peripheral clocks
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_SPI1EN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN | RCC_APB1ENR_USART2EN;
    
    // GPIO: PA0 as input with pull-up, PA1 as output
    GPIOA->CRL &= ~(0xF << (0*4));
    GPIOA->CRL |= (0x8 << (0*4));  // Input pull-up
    GPIOA->CRL &= ~(0xF << (1*4));
    GPIOA->CRL |= (0x3 << (1*4));  // Output 50MHz push-pull
    
    // ADC: 12-bit resolution, scan mode
    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_DMA | ADC_CR2_CONT;
    ADC1->SMPR2 = 0x00000007;  // 239.5 cycles sampling time
    
    // SPI: Master mode, CPOL=0, CPHA=0, 1MHz
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_2 | SPI_CR1_SPE;
    SPI1->CR2 = SPI_CR2_RXNEIE;  // RX interrupt
    
    // I2C: 100kHz standard mode
    I2C1->CR1 = 0;  // Disable to configure
    I2C1->CR2 = 36;  // 36 MHz peripheral clock
    I2C1->CCR = 180;  // 100kHz
    I2C1->TRISE = 37;
    I2C1->CR1 = I2C_CR1_PE;  // Enable
    
    // UART: 115200 baud @ 72MHz
    USART2->BRR = 0x0271;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
    
    // Enable interrupts in NVIC
    NVIC_EnableIRQ(ADC_IRQn);
    NVIC_EnableIRQ(SPI1_IRQn);
    NVIC_EnableIRQ(USART2_IRQn);
}
```

## What to Initialize

Your `lq_platform_peripherals_init()` should configure:

### 1. ADC (Analog-to-Digital Converter)
- Clock source and prescaler
- Resolution (8/10/12-bit)
- Sampling time
- DMA or interrupt mode
- Channel configuration

### 2. SPI (Serial Peripheral Interface)
- Master/slave mode
- Clock polarity and phase (CPOL/CPHA)
- Baud rate
- Data size (8/16-bit)
- DMA or interrupt mode

### 3. I2C (Inter-Integrated Circuit)
- Master mode
- Clock speed (100kHz/400kHz)
- Addressing mode (7/10-bit)
- Timeout configuration

### 4. UART (Universal Asynchronous Receiver/Transmitter)
- Baud rate
- Data bits (7/8/9)
- Parity (none/even/odd)
- Stop bits (1/2)
- Flow control (none/RTS-CTS)

### 5. GPIO (General Purpose I/O)
- Pin mode (input/output/alternate function)
- Pull-up/pull-down resistors
- Output speed
- Initial state

### 6. CAN (Controller Area Network)
- Bit timing (baud rate)
- Filter configuration
- Operating mode (normal/loopback/silent)
- Interrupts

### 7. Timers
- Clock source and prescaler
- Period/frequency
- PWM mode and duty cycle
- Interrupt configuration

## DTS Properties Reference

While you configure peripherals manually, the DTS file documents which hardware resources your application uses:

```dts
rpm_adc: rpm-sensor {
    compatible = "lq,hw-adc-input";
    signal-id = <0>;
    hw-instance = <1>;      // ADC1
    hw-channel = <0>;       // Channel 0
};

spi_sensor: position-sensor {
    compatible = "lq,hw-spi-input";
    signal-id = <1>;
    hw-instance = <1>;      // SPI1
};

i2c_temp: temperature-sensor {
    compatible = "lq,hw-i2c-input";
    signal-id = <2>;
    hw-instance = <1>;      // I2C1
    i2c-address = <0x48>;   // Device address
};

uart_debug: debug-port {
    compatible = "lq,hw-uart-input";
    signal-id = <3>;
    hw-instance = <2>;      // UART2
};

status_led: status-led {
    compatible = "lq,gpio-output";
    target-id = <0>;
    hw-port = "A";          // GPIOA
    hw-pin = <5>;           // Pin 5
};
```

These properties serve as documentation and can be used by code generation tools, but **you are responsible** for the actual peripheral initialization in `lq_platform_peripherals_init()`.

## Common Patterns

### Pattern 1: Vendor HAL + Wrapper
**Best for**: Production code with vendor tools

1. Use vendor tools (CubeMX, ESP-IDF, Atmel START)
2. Generate initialization code
3. Call from your wrapper function

**Pros**: Tested, supported, GUI configuration
**Cons**: Code bloat, vendor lock-in

### Pattern 2: Direct Register Access
**Best for**: Size-constrained or performance-critical applications

1. Read MCU reference manual
2. Write registers directly
3. Maximum control and efficiency

**Pros**: Small code, full control, portable
**Cons**: More code to write, harder to maintain

### Pattern 3: Minimal HAL
**Best for**: Balance between patterns 1 and 2

1. Use vendor HAL only for complex peripherals (USB, Ethernet)
2. Direct register access for simple peripherals (GPIO, ADC)
3. Mix and match as needed

**Pros**: Flexibility, reasonable code size
**Cons**: Need to understand both approaches

## Troubleshooting

### Peripheral Not Working

1. **Check clock enable**: Most MCUs require explicitly enabling peripheral clocks
   ```c
   RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;  // STM32 example
   ```

2. **Verify pin configuration**: Ensure GPIO pins are configured correctly
   ```c
   // STM32 HAL
   GPIO_InitTypeDef gpio = {0};
   gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;  // I2C SCL, SDA
   gpio.Mode = GPIO_MODE_AF_OD;          // Alternate function open-drain
   gpio.Pull = GPIO_PULLUP;
   HAL_GPIO_Init(GPIOB, &gpio);
   ```

3. **Check interrupt priorities**: Ensure interrupts don't conflict
   ```c
   HAL_NVIC_SetPriority(SPI1_IRQn, 5, 0);
   HAL_NVIC_EnableIRQ(SPI1_IRQn);
   ```

4. **Verify baud/clock rates**: Check calculations match your system clock
   ```c
   // UART baud = f_peripheral / (16 * BRR)
   // For 115200 @ 72MHz: BRR = 72000000 / (16 * 115200) = 39.0625
   ```

### Initialization Order

Some peripherals depend on others being initialized first:

```c
void lq_platform_peripherals_init(void)
{
    // 1. System clock configuration (usually in SystemInit)
    // 2. Enable peripheral clocks
    // 3. Configure GPIOs
    // 4. Configure peripherals
    // 5. Enable interrupts
    // 6. Start peripherals (DMA, timers, etc.)
}
```

### Using Without LQ_PLATFORM_INIT

If you need different initialization logic:

1. **Don't define** `LQ_PLATFORM_INIT`
2. Call your init **before** `lq_generated_init()`:

```c
int main(void) {
    // Your custom initialization
    my_custom_peripheral_init();
    
    // Then initialize LQ engine
    lq_generated_init();
    
    // Run application
    lq_engine_run();
}
```

## Platform-Specific Notes

### STM32
- **CubeMX integration**: Use MX_*_Init() functions
- **HAL callbacks**: Implement HAL callbacks for interrupts
- **Clock tree**: Configure in CubeMX or SystemClock_Config()

### ESP32
- **Component model**: Include component headers from ESP-IDF
- **FreeRTOS**: ESP-IDF includes FreeRTOS
- **TWAI**: CAN is called TWAI (Two-Wire Automotive Interface)

### Nordic nRF52
- **SoftDevice**: If using BLE, initialize SoftDevice first
- **TWI**: I2C is called TWI (Two-Wire Interface)
- **SAADC**: Successive Approximation ADC with unique API

### Atmel SAMD
- **Atmel START**: Web-based code configurator
- **ASF4**: Atmel Software Framework
- **Descriptors**: Use descriptor-based driver model

## Next Steps

- See [platform-adaptors.md](platform-adaptors.md) for auto-generation options
- See [platform-quick-reference.md](platform-quick-reference.md) for API reference
- See [samples/](../samples/) for complete examples
