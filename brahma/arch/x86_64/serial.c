#include "parama/serial.h"
#include <stdint.h>
#define COM1 0x3f8u
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" :: "a"(data), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
void serial_init(void) {
    outb(COM1 + 1, 0x00); /* disable UART interrupts */
    outb(COM1 + 3, 0x80); /* divisor latch access */
    outb(COM1 + 0, 0x01); /* baud divisor low: 115200 */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8N1 */
    outb(COM1 + 2, 0xc7); /* FIFO enabled, clear, 14-byte threshold */
    outb(COM1 + 4, 0x03); /* DTR and RTS */
}
void serial_putc(char ch) {
    /* Bounded UART polling: never wedge the kernel if the port disappears. */
    for (uint32_t spins = 0; spins < 100000u; ++spins) {
        if ((inb(COM1 + 5) & 0x20u) != 0) {
            outb(COM1, (uint8_t)ch);
            return;
        }
    }
}
void serial_write(const char *text) {
    if (text == 0) return;
    for (; *text; ++text) {
        if (*text == '\n') serial_putc('\r');
        serial_putc(*text);
    }
}
void serial_line(const char *text) { serial_write(text); serial_write("\n"); }
void serial_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    serial_write("0x");
    for (int i = 15; i >= 0; --i) serial_putc(digits[(value >> (i * 4)) & 15u]);
}
void serial_dec64(uint64_t value) {
    char buf[20];
    unsigned used = 0;
    do { buf[used++] = (char)('0' + (value % 10u)); value /= 10u; } while (value != 0);
    while (used) serial_putc(buf[--used]);
}
