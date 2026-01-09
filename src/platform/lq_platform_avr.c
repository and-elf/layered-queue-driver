/*
 * Copyright (c) 2026 Layered Queue Driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * AVR Platform Implementation (ATmega328P, ATmega2560, etc.)
 */

#include "lq_platform.h"

#ifdef __AVR__

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* =============================================================================
 * UART Platform Functions (AVR)
 * ========================================================================== */

int lq_uart_send(uint8_t port, const uint8_t *data, uint16_t length)
{
    /* AVR typically has UART0, some have UART1-3 */
    volatile uint8_t *udr;
    volatile uint8_t *ucsra;
    uint8_t udre_bit;
    
    switch (port) {
#ifdef UDR0
        case 0:
            udr = &UDR0;
            ucsra = &UCSR0A;
            udre_bit = UDRE0;
            break;
#endif
#ifdef UDR1
        case 1:
            udr = &UDR1;
            ucsra = &UCSR1A;
            udre_bit = UDRE1;
            break;
#endif
        default:
            return -ENODEV;
    }
    
    for (uint16_t i = 0; i < length; i++) {
        /* Wait for empty transmit buffer */
        while (!(*ucsra & (1 << udre_bit)));
        *udr = data[i];
    }
    
    return 0;
}

int lq_uart_recv(uint8_t port, uint8_t *data, uint16_t length, uint32_t timeout_ms)
{
    volatile uint8_t *udr;
    volatile uint8_t *ucsra;
    uint8_t rxc_bit;
    
    switch (port) {
#ifdef UDR0
        case 0:
            udr = &UDR0;
            ucsra = &UCSR0A;
            rxc_bit = RXC0;
            break;
#endif
#ifdef UDR1
        case 1:
            udr = &UDR1;
            ucsra = &UCSR1A;
            rxc_bit = RXC1;
            break;
#endif
        default:
            return -ENODEV;
    }
    
    /* Simple timeout implementation - not precise */
    uint32_t timeout_loops = timeout_ms * 100;
    
    for (uint16_t i = 0; i < length; i++) {
        uint32_t wait = 0;
        while (!(*ucsra & (1 << rxc_bit))) {
            if (++wait > timeout_loops) {
                return -ETIMEDOUT;
            }
            _delay_us(10);
        }
        data[i] = *udr;
    }
    
    return length;
}

/* =============================================================================
 * GPIO Platform Functions (AVR)
 * ========================================================================== */

/* Pin mapping: pin_id = port_num * 8 + bit_num
 * Port A = 0-7, Port B = 8-15, Port C = 16-23, Port D = 24-31, etc.
 */

static volatile uint8_t* get_port_reg(uint8_t pin)
{
    uint8_t port_num = pin / 8;
    
#ifdef PORTA
    if (port_num == 0) return &PORTA;
#endif
#ifdef PORTB
    if (port_num == 1) return &PORTB;
#endif
#ifdef PORTC
    if (port_num == 2) return &PORTC;
#endif
#ifdef PORTD
    if (port_num == 3) return &PORTD;
#endif
#ifdef PORTE
    if (port_num == 4) return &PORTE;
#endif
#ifdef PORTF
    if (port_num == 5) return &PORTF;
#endif
    return NULL;
}

static volatile uint8_t* get_pin_reg(uint8_t pin)
{
    uint8_t port_num = pin / 8;
    
#ifdef PINA
    if (port_num == 0) return &PINA;
#endif
#ifdef PINB
    if (port_num == 1) return &PINB;
#endif
#ifdef PINC
    if (port_num == 2) return &PINC;
#endif
#ifdef PIND
    if (port_num == 3) return &PIND;
#endif
#ifdef PINE
    if (port_num == 4) return &PINE;
#endif
#ifdef PINF
    if (port_num == 5) return &PINF;
#endif
    return NULL;
}

int lq_gpio_set(uint8_t pin, bool value)
{
    volatile uint8_t *port = get_port_reg(pin);
    if (!port) {
        return -ENODEV;
    }
    
    uint8_t bit = pin % 8;
    
    if (value) {
        *port |= (1 << bit);
    } else {
        *port &= ~(1 << bit);
    }
    
    return 0;
}

int lq_gpio_get(uint8_t pin, bool *value)
{
    volatile uint8_t *pin_reg = get_pin_reg(pin);
    if (!pin_reg || !value) {
        return -ENODEV;
    }
    
    uint8_t bit = pin % 8;
    *value = (*pin_reg & (1 << bit)) != 0;
    
    return 0;
}

int lq_gpio_toggle(uint8_t pin)
{
    volatile uint8_t *port = get_port_reg(pin);
    if (!port) {
        return -ENODEV;
    }
    
    uint8_t bit = pin % 8;
    *port ^= (1 << bit);
    
    return 0;
}

/* =============================================================================
 * PWM Platform Functions (AVR)
 * Uses Timer0, Timer1, Timer2 for PWM generation
 * ========================================================================== */

int lq_pwm_set(uint8_t channel, uint16_t duty_cycle, uint32_t frequency_hz)
{
    /* AVR PWM channels map to timer compare outputs:
     * Channel 0: OC0A (Timer0 channel A)
     * Channel 1: OC0B (Timer0 channel B)
     * Channel 2: OC1A (Timer1 channel A)
     * Channel 3: OC1B (Timer1 channel B)
     * Channel 4: OC2A (Timer2 channel A)
     * Channel 5: OC2B (Timer2 channel B)
     */
    
    (void)frequency_hz;  /* AVR PWM frequency is typically fixed by timer prescaler */
    
    /* Convert duty_cycle (0-10000) to 8-bit value (0-255) for 8-bit timers */
    uint8_t ocr_value = (duty_cycle * 255) / 10000;
    
    switch (channel) {
#ifdef OCR0A
        case 0:
            /* Timer0 Channel A */
            TCCR0A |= (1 << COM0A1);  /* Non-inverting mode */
            TCCR0A |= (1 << WGM00) | (1 << WGM01);  /* Fast PWM */
            TCCR0B |= (1 << CS01);  /* Prescaler = 8 */
            OCR0A = ocr_value;
            break;
#endif
#ifdef OCR0B
        case 1:
            /* Timer0 Channel B */
            TCCR0A |= (1 << COM0B1);
            TCCR0A |= (1 << WGM00) | (1 << WGM01);
            TCCR0B |= (1 << CS01);
            OCR0B = ocr_value;
            break;
#endif
#ifdef OCR1A
        case 2:
            /* Timer1 Channel A (16-bit timer) */
            TCCR1A |= (1 << COM1A1);
            TCCR1A |= (1 << WGM10);  /* 8-bit fast PWM for compatibility */
            TCCR1B |= (1 << WGM12) | (1 << CS11);  /* Prescaler = 8 */
            OCR1A = ocr_value;
            break;
#endif
#ifdef OCR1B
        case 3:
            /* Timer1 Channel B */
            TCCR1A |= (1 << COM1B1);
            TCCR1A |= (1 << WGM10);
            TCCR1B |= (1 << WGM12) | (1 << CS11);
            OCR1B = ocr_value;
            break;
#endif
#ifdef OCR2A
        case 4:
            /* Timer2 Channel A */
            TCCR2A |= (1 << COM2A1);
            TCCR2A |= (1 << WGM20) | (1 << WGM21);
            TCCR2B |= (1 << CS21);
            OCR2A = ocr_value;
            break;
#endif
#ifdef OCR2B
        case 5:
            /* Timer2 Channel B */
            TCCR2A |= (1 << COM2B1);
            TCCR2A |= (1 << WGM20) | (1 << WGM21);
            TCCR2B |= (1 << CS21);
            OCR2B = ocr_value;
            break;
#endif
        default:
            return -ENODEV;
    }
    
    return 0;
}

/* =============================================================================
 * SPI Platform Functions (AVR)
 * ========================================================================== */

int lq_spi_send(uint8_t cs_pin, const uint8_t *data, uint16_t length)
{
    /* Pull CS low */
    lq_gpio_set(cs_pin, false);
    
    for (uint16_t i = 0; i < length; i++) {
        /* Start transmission */
        SPDR = data[i];
        
        /* Wait for transmission complete */
        while (!(SPSR & (1 << SPIF)));
    }
    
    /* Pull CS high */
    lq_gpio_set(cs_pin, true);
    
    return 0;
}

int lq_spi_recv(uint8_t cs_pin, uint8_t *data, uint16_t length)
{
    lq_gpio_set(cs_pin, false);
    
    for (uint16_t i = 0; i < length; i++) {
        /* Send dummy byte to generate clock */
        SPDR = 0xFF;
        
        /* Wait for reception complete */
        while (!(SPSR & (1 << SPIF)));
        
        data[i] = SPDR;
    }
    
    lq_gpio_set(cs_pin, true);
    
    return length;
}

int lq_spi_transceive(uint8_t cs_pin, const uint8_t *tx_data, uint8_t *rx_data, uint16_t length)
{
    lq_gpio_set(cs_pin, false);
    
    for (uint16_t i = 0; i < length; i++) {
        SPDR = tx_data[i];
        while (!(SPSR & (1 << SPIF)));
        rx_data[i] = SPDR;
    }
    
    lq_gpio_set(cs_pin, true);
    
    return 0;
}

/* =============================================================================
 * I2C Platform Functions (AVR)
 * Uses TWI (Two Wire Interface) peripheral
 * ========================================================================== */

#define TWI_START       0x08
#define TWI_REP_START   0x10
#define TWI_MT_SLA_ACK  0x18
#define TWI_MT_DATA_ACK 0x28
#define TWI_MR_SLA_ACK  0x40
#define TWI_MR_DATA_ACK 0x50
#define TWI_MR_DATA_NACK 0x58

static int twi_start(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    
    uint8_t status = TWSR & 0xF8;
    if (status != TWI_START && status != TWI_REP_START) {
        return -EIO;
    }
    return 0;
}

static void twi_stop(void)
{
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

static int twi_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    return 0;
}

static uint8_t twi_read_ack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

static uint8_t twi_read_nack(void)
{
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT)));
    return TWDR;
}

int lq_i2c_write(uint8_t address, const uint8_t *data, uint16_t length)
{
    if (twi_start() != 0) return -EIO;
    
    twi_write((address << 1) | 0x00);  /* Write mode */
    
    for (uint16_t i = 0; i < length; i++) {
        twi_write(data[i]);
    }
    
    twi_stop();
    return 0;
}

int lq_i2c_read(uint8_t address, uint8_t *data, uint16_t length)
{
    if (twi_start() != 0) return -EIO;
    
    twi_write((address << 1) | 0x01);  /* Read mode */
    
    for (uint16_t i = 0; i < length; i++) {
        if (i == length - 1) {
            data[i] = twi_read_nack();  /* Last byte: NACK */
        } else {
            data[i] = twi_read_ack();   /* ACK for more data */
        }
    }
    
    twi_stop();
    return 0;
}

int lq_i2c_write_read(uint8_t address,
                      const uint8_t *write_data, uint16_t write_length,
                      uint8_t *read_data, uint16_t read_length)
{
    /* Write phase */
    if (twi_start() != 0) return -EIO;
    twi_write((address << 1) | 0x00);
    for (uint16_t i = 0; i < write_length; i++) {
        twi_write(write_data[i]);
    }
    
    /* Read phase with repeated start */
    if (twi_start() != 0) return -EIO;
    twi_write((address << 1) | 0x01);
    for (uint16_t i = 0; i < read_length; i++) {
        if (i == read_length - 1) {
            read_data[i] = twi_read_nack();
        } else {
            read_data[i] = twi_read_ack();
        }
    }
    
    twi_stop();
    return 0;
}

int lq_i2c_reg_write_byte(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return lq_i2c_write(address, data, 2);
}

int lq_i2c_reg_read_byte(uint8_t address, uint8_t reg, uint8_t *value)
{
    return lq_i2c_write_read(address, &reg, 1, value, 1);
}

int lq_i2c_burst_write(uint8_t address, uint8_t start_reg, const uint8_t *data, uint16_t length)
{
    if (twi_start() != 0) return -EIO;
    
    twi_write((address << 1) | 0x00);
    twi_write(start_reg);
    
    for (uint16_t i = 0; i < length; i++) {
        twi_write(data[i]);
    }
    
    twi_stop();
    return 0;
}

int lq_i2c_burst_read(uint8_t address, uint8_t start_reg, uint8_t *data, uint16_t length)
{
    return lq_i2c_write_read(address, &start_reg, 1, data, length);
}

int lq_i2c_set_default_bus(uint8_t bus_id)
{
    /* AVR typically has only one TWI peripheral */
    (void)bus_id;
    return 0;
}

#endif /* __AVR__ */
