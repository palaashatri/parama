#ifndef PARAMA_SERIAL_H
#define PARAMA_SERIAL_H
#include <stdint.h>
void serial_init(void);
void serial_putc(char ch);
void serial_write(const char *text);
void serial_hex64(uint64_t value);
void serial_dec64(uint64_t value);
void serial_line(const char *text);
#endif
