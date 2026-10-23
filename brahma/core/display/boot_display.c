#include "parama/display.h"
#include <stdint.h>
#include <stddef.h>
/* Initial kernel-rendered diagnostic splash. This is NOT SURYA or a compositor. */
static uint32_t channel(uint8_t val, uint8_t bits, uint8_t shift) {
    if (bits == 0 || bits > 8 || shift > 24 || (unsigned)bits + shift > 32) return 0;
    uint32_t scale = (1u << bits) - 1u;
    return ((uint32_t)val * scale / 255u) << shift;
}
static uint32_t color(struct limine_framebuffer *fb, uint8_t r, uint8_t g, uint8_t b) {
    return channel(r, fb->red_mask_size, fb->red_mask_shift)
        | channel(g, fb->green_mask_size, fb->green_mask_shift)
        | channel(b, fb->blue_mask_size, fb->blue_mask_shift);
}
static void rect(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                 uint64_t w, uint64_t h, uint32_t rgb) {
    if (x >= fb->width || y >= fb->height) return;
    if (w > fb->width - x) w = fb->width - x;
    if (h > fb->height - y) h = fb->height - y;
    volatile uint8_t *base = (volatile uint8_t *)fb->address;
    for (uint64_t iy = y; iy < y + h; ++iy) {
        volatile uint32_t *row = (volatile uint32_t *)(base + iy * fb->pitch);
        for (uint64_t ix = x; ix < x + w; ++ix) row[ix] = rgb;
    }
}
/* Tiny deterministic glyphs sufficient for a recognizable diagnostic image. */
typedef struct { char c; uint8_t col[5]; } glyph_t;
static const glyph_t glyphs[] = {
    { 'A', {0x7e,0x11,0x11,0x11,0x7e} },
    { 'B', {0x7f,0x49,0x49,0x49,0x36} },
    { 'D', {0x7f,0x41,0x41,0x22,0x1c} },
    { 'E', {0x7f,0x49,0x49,0x49,0x41} },
    { 'H', {0x7f,0x08,0x08,0x08,0x7f} },
    { 'I', {0x41,0x41,0x7f,0x41,0x41} },
    { 'K', {0x7f,0x08,0x14,0x22,0x41} },
    { 'L', {0x7f,0x40,0x40,0x40,0x40} },
    { 'M', {0x7f,0x02,0x0c,0x02,0x7f} },
    { 'O', {0x3e,0x41,0x41,0x41,0x3e} },
    { 'P', {0x7f,0x09,0x09,0x09,0x06} },
    { 'R', {0x7f,0x09,0x19,0x29,0x46} },
    { 'S', {0x46,0x49,0x49,0x49,0x31} },
    { 'T', {0x01,0x01,0x7f,0x01,0x01} },
    { 'V', {0x1f,0x20,0x40,0x20,0x1f} },
    { '0', {0x3e,0x45,0x49,0x51,0x3e} },
    { '1', {0x00,0x42,0x7f,0x40,0x00} },
    { '.', {0x00,0x60,0x60,0x00,0x00} },
    { '-', {0x08,0x08,0x08,0x08,0x08} },
    { ' ', {0,0,0,0,0} }
};
static void label(struct limine_framebuffer *fb, uint64_t x, uint64_t y,
                  const char *text, unsigned scale, uint32_t paint) {
    for (; *text; ++text) {
        const glyph_t *g = &glyphs[sizeof(glyphs) / sizeof(glyphs[0]) - 1];
        for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); ++i) {
            if (glyphs[i].c == *text) { g = &glyphs[i]; break; }
        }
        for (uint64_t cx = 0; cx < 5; ++cx) {
            for (uint64_t cy = 0; cy < 7; ++cy) {
                if (g->col[cx] & (1u << cy))
                    rect(fb, x + cx * scale, y + cy * scale, scale, scale, paint);
            }
        }
        x += 6u * scale;
    }
}
bool boot_display_draw(struct limine_framebuffer *fb) {
    if (!fb || !fb->address || fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->bpp != 32
        || fb->width < 320 || fb->height < 200 || fb->width > UINT64_MAX / 4u
        || fb->pitch < fb->width * 4u || fb->height > UINT64_MAX / fb->pitch) return false;
    uint32_t background = color(fb, 13, 19, 32);
    uint32_t accent = color(fb, 126, 87, 236);
    uint32_t white = color(fb, 240, 242, 255);
    uint32_t gray = color(fb, 145, 155, 177);
    rect(fb, 0, 0, fb->width, fb->height, background);
    rect(fb, 0, 0, fb->width, 8, accent);
    rect(fb, 40, 54, 54, 54, accent);
    rect(fb, 54, 68, 26, 26, background);
    label(fb, 112, 52, "PARAMA OS", 4, white);
    label(fb, 112, 91, "BRAHMA BOOT", 2, gray);
    rect(fb, 40, 144, fb->width > 80 ? fb->width - 80 : 0, 1, gray);
    label(fb, 40, 166, "KERNEL ONLINE", 2, white);
    return true;
}
