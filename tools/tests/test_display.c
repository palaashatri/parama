#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "parama/display.h"
static uint32_t pixels[400 * 240 + 4];
/* Every non-space character cell must contain at least one pixel in the text
 * colour; catches characters missing from the glyph table (which silently
 * render as blanks). Cell = 5x7 glyph at `scale`, advancing 6*scale. */
static void assert_label_rendered(unsigned stride, unsigned x, unsigned y, unsigned scale,
                                  const char *text, uint32_t paint) {
    for (unsigned i = 0; text[i]; ++i) {
        if (text[i] == ' ') continue;
        unsigned cx0 = x + i * 6u * scale, found = 0;
        for (unsigned py = y; py < y + 7u * scale && !found; ++py)
            for (unsigned px = cx0; px < cx0 + 5u * scale && px < stride; ++px)
                if (pixels[py * stride + px] == paint) { found = 1; break; }
        if (!found) fprintf(stderr, "glyph '%c' (index %u) of \"%s\" not drawn\n", text[i], i, text);
        assert(found);
    }
}
int main(void) {
    const uint32_t canary = UINT32_C(0xdeadbeef);
    for (unsigned i = 0; i < 4; ++i) pixels[400 * 240 + i] = canary;
    struct limine_framebuffer fb = {
        .address = pixels, .width = 400, .height = 240, .pitch = 400 * 4,
        .bpp = 32, .memory_model = LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size = 8, .red_mask_shift = 16,
        .green_mask_size = 8, .green_mask_shift = 8,
        .blue_mask_size = 8, .blue_mask_shift = 0
    };
    assert(boot_display_draw(&fb));
    assert(pixels[0] != pixels[400 * 100]); /* accent stripe */
    assert(pixels[400 * 60 + 50] != pixels[400 * 120 + 50]); /* logo */
    assert(pixels[400 * 150 + 100] != pixels[400 * 144 + 100]);
    for (unsigned i = 0; i < 4; ++i) assert(pixels[400 * 240 + i] == canary);
    /* Colours as encoded by boot_display.c for an XRGB8888 framebuffer. */
    const uint32_t white = (240u << 16) | (242u << 8) | 255u;
    const uint32_t gray = (145u << 16) | (155u << 8) | 177u;
    assert_label_rendered(400, 112, 52, 4, "PARAMA OS", white);
    assert_label_rendered(400, 112, 91, 2, "BRAHMA BOOT", gray);
    assert_label_rendered(400, 40, 166, 2, "KERNEL ONLINE", white);

    /* Smallest supported mode (M1 gate names 320x200): all text drawn, none clipped. */
    for (unsigned i = 0; i < 400 * 240; ++i) pixels[i] = 0;
    fb.width = 320; fb.height = 200; fb.pitch = 320 * 4;
    assert(boot_display_draw(&fb));
    assert_label_rendered(320, 112, 52, 3, "PARAMA OS", white);
    assert_label_rendered(320, 112, 91, 2, "BRAHMA BOOT", gray);
    assert_label_rendered(320, 40, 166, 2, "KERNEL ONLINE", white);
    for (unsigned y = 0; y < 200; ++y) {
        uint32_t edge = pixels[y * 320 + 319];
        if (edge == white || edge == gray) fprintf(stderr, "text clipped at right edge, row %u\n", y);
        assert(edge != white && edge != gray);
    }
    for (unsigned i = 320 * 200; i < 400 * 240; ++i) assert(pixels[i] == 0);
    for (unsigned i = 0; i < 4; ++i) assert(pixels[400 * 240 + i] == canary);
    fb.width = 400; fb.height = 240;
    fb.pitch = 100;
    assert(!boot_display_draw(&fb));
    fb.pitch = 400 * 4;
    fb.bpp = 24;
    assert(!boot_display_draw(&fb));
    puts("[UNIT][PASS] BRAHMA framebuffer bounds and rendering");
    return 0;
}
