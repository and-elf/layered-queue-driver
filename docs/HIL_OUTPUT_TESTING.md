# HIL Output Testing

This document describes the output types that can be verified in Hardware-in-the-Loop (HIL) tests.

## Supported Output Types

The HIL testing infrastructure now supports verification of the following output types:

### 1. CAN Messages
- **Function**: `lq_can_send(can_id, is_extended, data, length)`
- **Test Function**: `lq_hil_tester_wait_can(msg, timeout_ms)`
- **Description**: Verify CAN message transmission with ID, extended flag, and data payload
- **Use Cases**: Automotive communication, J1939, CANopen

### 2. GPIO Outputs
- **Function**: `lq_gpio_set(pin, value)`
- **Test Function**: `lq_hil_tester_wait_gpio(ops, pin, expected_state, timeout_ms)`
- **Description**: Verify digital output pin states
- **Use Cases**: LED control, relay actuation, digital signaling

### 3. UART/Serial Output
- **Function**: `lq_uart_send(port, data, length)`
- **Test Function**: `lq_hil_tester_wait_uart(msg, timeout_ms)`
- **Description**: Verify serial data transmission
- **Use Cases**: Debug logging, Modbus RTU, serial protocols

### 4. PWM Outputs
- **Function**: `lq_pwm_set(channel, duty_cycle, frequency_hz)`
- **Test Function**: `lq_hil_tester_wait_pwm(msg, timeout_ms)`
- **Description**: Verify PWM duty cycle (0-10000 = 0-100.00%) and frequency
- **Use Cases**: Motor control, LED dimming, servo control

### 5. SPI Output
- **Function**: `lq_spi_send(cs_pin, data, length)`
- **Test Function**: `lq_hil_tester_wait_spi_out(msg, timeout_ms)`
- **Description**: Verify SPI transmissions with chip select and data
- **Use Cases**: Sensor configuration, external ADC/DAC, displays

### 6. I2C Transactions
- **Function**: `lq_i2c_write(address, data, length)` / `lq_i2c_read(address, data, length)`
- **Test Function**: `lq_hil_tester_wait_i2c(msg, timeout_ms)`
- **Description**: Verify I2C read/write operations
- **Use Cases**: Sensor communication, EEPROM access, peripheral control

## How It Works

The HIL infrastructure intercepts platform function calls and routes them through Unix domain sockets:

1. **SUT (System Under Test)** runs your application code
2. When your code calls an output function (e.g., `lq_uart_send()`), it's intercepted
3. The data is sent via socket to the **Test Runner**
4. Test cases wait for and verify the outputs using `lq_hil_tester_wait_*()` functions

## Message Structure

Each output type has a corresponding message structure:

```c
struct lq_hil_uart_msg {
    uint8_t port;           // UART port number
    uint16_t length;        // Data length
    uint8_t data[256];      // Data bytes
};

struct lq_hil_pwm_msg {
    uint8_t channel;        // PWM channel
    uint16_t duty_cycle;    // 0-10000 (0-100.00%)
    uint32_t frequency_hz;  // Frequency in Hz
};

struct lq_hil_spi_out_msg {
    uint8_t cs_pin;         // Chip select pin
    uint16_t length;        // Data length
    uint8_t data[256];      // Data bytes
};

struct lq_hil_i2c_msg {
    uint8_t address;        // I2C device address
    uint8_t is_read;        // 0=write, 1=read
    uint16_t length;        // Data length
    uint8_t data[256];      // Data bytes
};
```

## Example Test

```cpp
TEST(MyAppTest, VerifyUartOutput) {
    // Inject ADC input to trigger UART output
    struct lq_hil_adc_msg adc_msg = {
        .hdr = {.type = LQ_HIL_MSG_ADC, .timestamp_us = 0, .channel = 0},
        .value_mv = 2500
    };
    lq_hil_tester_send_adc(NULL, &adc_msg);
    
    // Wait for UART output
    struct lq_hil_uart_msg uart_msg;
    ASSERT_EQ(0, lq_hil_tester_wait_uart(&uart_msg, 1000));
    
    // Verify the output
    EXPECT_EQ(0, uart_msg.port);  // UART0
    EXPECT_GT(uart_msg.length, 0);
    EXPECT_STREQ((char*)uart_msg.data, "Voltage: 2.50V\n");
}

TEST(MotorTest, VerifyPwmControl) {
    // Trigger motor control
    // ... inject inputs ...
    
    // Wait for PWM update
    struct lq_hil_pwm_msg pwm_msg;
    ASSERT_EQ(0, lq_hil_tester_wait_pwm(&pwm_msg, 1000));
    
    // Verify PWM settings
    EXPECT_EQ(0, pwm_msg.channel);        // Motor channel 0
    EXPECT_EQ(7500, pwm_msg.duty_cycle);  // 75% duty cycle
    EXPECT_EQ(1000, pwm_msg.frequency_hz); // 1 kHz
}
```

## Socket Communication

Each output type uses a dedicated Unix domain socket:

- **CAN**: `/tmp/lq_hil_can_<pid>`
- **GPIO**: `/tmp/lq_hil_gpio_<pid>`
- **UART**: `/tmp/lq_hil_uart_<pid>`
- **PWM**: `/tmp/lq_hil_pwm_<pid>`
- **SPI**: `/tmp/lq_hil_spi_out_<pid>`
- **I2C**: `/tmp/lq_hil_i2c_<pid>`

The sockets are automatically created during `lq_hil_init()` and cleaned up on shutdown.

## Automatic Test Generation

When you enable HIL testing in CMake:

```cmake
add_lq_application(my_app
    DTS app.dts
    PLATFORM native
    ENABLE_HIL_TESTS
)
```

The build system automatically:
1. Parses your DTS file to identify outputs
2. Generates a test runner with verification code
3. Creates SUT and test runner executables
4. Provides a `my_app_hil_run` target to run the tests

## Best Practices

1. **Timeout Values**: Use reasonable timeouts (100-1000ms) to avoid flaky tests
2. **Output Ordering**: Don't assume strict output ordering unless guaranteed by your application
3. **Data Verification**: Verify both the fact that output occurred and its content/parameters
4. **Edge Cases**: Test boundary conditions (max data length, min/max PWM duty cycle, etc.)
5. **Cleanup**: Tests automatically clean up sockets, but ensure proper HIL shutdown in your SUT

## Implementation Files

- **Header**: [include/lq_hil.h](../include/lq_hil.h)
- **Implementation**: [src/drivers/lq_hil.c](../src/drivers/lq_hil.c)
- **Platform Wrappers**: [src/platform/lq_platform_hil.c](../src/platform/lq_platform_hil.c)
- **Test Utilities**: [tests/hil/](../tests/hil/)
