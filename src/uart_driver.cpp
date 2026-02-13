/**
 * @file uart_driver.cpp
 * @brief UART driver implementation for ATmega328P
 */

#include "uart_driver.h"
#include <avr/pgmspace.h>

void uart_init(uint32_t baud, uint32_t f_cpu) {
    // Calculate UBRR value for given baud rate
    // UBRR = (F_CPU / (16 * BAUD)) - 1
    uint16_t ubrr = (f_cpu / (16UL * baud)) - 1;
    
    // Set baud rate registers
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    
    // Enable transmitter only (we don't need receiver)
    UCSR0B = (1 << TXEN0);
    
    // Set frame format: 8 data bits, 1 stop bit, no parity (8N1)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void uart_putc(char data) {
    // Wait for empty transmit buffer
    while (!(UCSR0A & (1 << UDRE0)));
    
    // Put data into buffer, sends the data
    UDR0 = data;
}

void uart_puts(const char* str) {
    while (*str) {
        uart_putc(*str++);
    }
}

void uart_puts_P(const char* str) {
    char c;
    while ((c = pgm_read_byte(str++))) {
        uart_putc(c);
    }
}

void uart_print_u16(uint16_t value) {
    // Static buffer to avoid stack allocation in recursion
    static char buffer[6]; // Max 5 digits + null
    char* ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';
    
    // Handle zero case
    if (value == 0) {
        uart_putc('0');
        return;
    }
    
    // Convert to string (reverse order)
    while (value > 0) {
        *--ptr = '0' + (value % 10);
        value /= 10;
    }
    
    uart_puts(ptr);
}

void uart_print_hex16(uint16_t value) {
    static const char hex_digits[] PROGMEM = "0123456789ABCDEF";
    
    uart_puts("0x");
    
    // Print 4 hex digits
    for (int8_t i = 12; i >= 0; i -= 4) {
        uint8_t digit = (value >> i) & 0x0F;
        uart_putc(pgm_read_byte(&hex_digits[digit]));
    }
}

void uart_print_float(float value) {
    // Handle negative values
    if (value < 0) {
        uart_putc('-');
        value = -value;
    }
    
    // Print integer part
    uint16_t int_part = (uint16_t)value;
    uart_print_u16(int_part);
    
    uart_putc('.');
    
    // Print one decimal place
    uint16_t frac_part = (uint16_t)((value - int_part) * 10);
    uart_putc('0' + (frac_part % 10));
}

void uart_newline(void) {
    uart_putc('\r');
    uart_putc('\n');
}
