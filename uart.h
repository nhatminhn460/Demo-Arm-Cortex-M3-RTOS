#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_print(const char *s);
void uart_print_dec(uint32_t val);
char uart_getc(void);
void UART0_Handler(void);
void uart_print_hex(uint8_t n);
void uart_print_hex32(uint32_t n);

#endif
