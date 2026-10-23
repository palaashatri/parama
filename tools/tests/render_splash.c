/* Host reference renderer: draws the BRAHMA M1 splash with the exact kernel
 * code (boot_display.c) into a WIDTHxHEIGHT XRGB8888 buffer and writes a
 * binary PPM. QA compares this pixel-for-pixel against a VM screendump.
 * Usage: render_splash WIDTH HEIGHT out.ppm */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "parama/display.h"
int main(int argc, char **argv) {
    if (argc != 4) { fprintf(stderr, "usage: %s WIDTH HEIGHT out.ppm\n", argv[0]); return 2; }
    unsigned long w = strtoul(argv[1], NULL, 10), h = strtoul(argv[2], NULL, 10);
    if (w == 0 || h == 0 || w > 16384 || h > 16384) { fputs("bad size\n", stderr); return 2; }
    uint32_t *px = calloc(w * h, sizeof *px);
    if (!px) return 1;
    struct limine_framebuffer fb = {
        .address = px, .width = w, .height = h, .pitch = w * 4,
        .bpp = 32, .memory_model = LIMINE_FRAMEBUFFER_RGB,
        .red_mask_size = 8, .red_mask_shift = 16,
        .green_mask_size = 8, .green_mask_shift = 8,
        .blue_mask_size = 8, .blue_mask_shift = 0
    };
    if (!boot_display_draw(&fb)) { fputs("boot_display_draw rejected mode\n", stderr); return 1; }
    FILE *out = fopen(argv[3], "wb");
    if (!out) { perror(argv[3]); return 1; }
    fprintf(out, "P6\n%lu %lu\n255\n", w, h);
    for (unsigned long i = 0; i < w * h; ++i) {
        unsigned char rgb[3] = { (unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8), (unsigned char)px[i] };
        fwrite(rgb, 1, 3, out);
    }
    free(px);
    return fclose(out) == 0 ? 0 : 1;
}
