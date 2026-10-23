#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "parama/display.h"
static uint32_t pixels[400 * 240 + 4];
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
    fb.pitch = 100;
    assert(!boot_display_draw(&fb));
    fb.pitch = 400 * 4;
    fb.bpp = 24;
    assert(!boot_display_draw(&fb));
    puts("[UNIT][PASS] BRAHMA framebuffer bounds and rendering");
    return 0;
}
