/**
 * @file uart_driver.h
 * @brief Lightweight UART driver for ATmega328P diagnostic output
 * 
 * Implements blocking UART transmission at 115200 baud for memory diagnostics.
 * No dynamic memory allocation. Uses static buffers only.
 * 
 * Hardware: USART0 on ATmega328P
 * - TX: PD1 (Arduino Digital Pin 1)
 * - RX: PD0 (Arduino Digital Pin 0) - Not used
 */

#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include <avr/io.h>
#include <stdint.h>

/**
 * @brief Initialize UART hardware
 * @param baud Baud rate (typically 115200)
 * @param f_cpu CPU frequency in Hz
 * 
 * Configures USART0 for 8N1 transmission.
 * Must be called before any UART operations.
 */
void uart_init(uint32_t baud, uint32_t f_cpu);

/**
 * @brief Transmit single byte (blocking)
 * @param data Byte to transmit
 */
void uart_putc(char data);

/**
 * @brief Transmit null-terminated string (blocking)
 * @param str Pointer to string
 */
void uart_puts(const char* str);

/**
 * @brief Transmit string from program memory (PROGMEM)
 * @param str Pointer to PROGMEM string
 */
void uart_puts_P(const char* str);

/**
 * @brief Print unsigned 16-bit integer as decimal
 * @param value Value to print
 */
void uart_print_u16(uint16_t value);

/**
 * @brief Print unsigned 16-bit integer as hexadecimal
 * @param value Value to print
 */
void uart_print_hex16(uint16_t value);

/**
 * @brief Print floating point with 1 decimal precision
 * @param value Float value to print
 * 
 * Optimized for fragmentation percentage display.
 * Avoids floating-point printf overhead.
 */
void uart_print_float(float value);

/**
 * @brief Print newline (CRLF)
 */
void uart_newline(void);

#endif // UART_DRIVER_H
