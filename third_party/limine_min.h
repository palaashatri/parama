/* SPDX-License-Identifier: 0BSD
 * Subset of Limine protocol structures required by PARAMA M0-M1.
 * Derived from https://github.com/limine-bootloader/limine-protocol
 * Keep in sync with the upstream protocol when upgrading bootloader.
 * This file must not be mistaken for the full upstream limine.h.
 */
#ifndef PARAMA_LIMINE_MIN_H
#define PARAMA_LIMINE_MIN_H
#include <stdint.h>
#define LIMINE_REQUESTS_START_MARKER { 0xf6b8f4b39de7d1aeULL, 0xfab91a6940fcb9cfULL, 0x785c6ed015d3e316ULL, 0x181e920a7852b9d9ULL }
#define LIMINE_REQUESTS_END_MARKER { 0xadc0e0531bb10d03ULL, 0x9572709f31764c62ULL }
#define LIMINE_BASE_REVISION(N) { 0xf9562b2d5c95a6c8ULL, 0x6a7b384944536bdcULL, (N) }
#define LIMINE_BASE_REVISION_SUPPORTED(VAR) ((VAR)[2] == 0)
#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88ULL, 0x0a82e883a194f07bULL
#define LIMINE_HHDM_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x48dcf1cb8ad2b852ULL, 0x63984e959a98244bULL }
#define LIMINE_MEMMAP_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806fULL, 0xe304acdfc50c3c62ULL }
#define LIMINE_FRAMEBUFFER_REQUEST_ID { LIMINE_COMMON_MAGIC, 0x9d5827dcd881dd75ULL, 0xa3148604f6fab11bULL }
#define LIMINE_BOOTLOADER_INFO_REQUEST_ID { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202fULL, 0x279426fcf5f59740ULL }
#define LIMINE_FRAMEBUFFER_RGB 1
#define LIMINE_MEMMAP_USABLE 0
struct limine_hhdm_response { uint64_t revision; uint64_t offset; };
struct limine_hhdm_request { uint64_t id[4]; uint64_t revision; struct limine_hhdm_response *response; };
struct limine_memmap_entry { uint64_t base; uint64_t length; uint64_t type; };
struct limine_memmap_response { uint64_t revision; uint64_t entry_count; struct limine_memmap_entry **entries; };
struct limine_memmap_request { uint64_t id[4]; uint64_t revision; struct limine_memmap_response *response; };
struct limine_framebuffer {
    void *address;
    uint64_t width, height, pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size, red_mask_shift;
    uint8_t green_mask_size, green_mask_shift;
    uint8_t blue_mask_size, blue_mask_shift;
    uint8_t unused[7];
    uint64_t edid_size;
    void *edid;
    uint64_t mode_count;
    void *modes;
};
struct limine_framebuffer_response {
    uint64_t revision;
    uint64_t framebuffer_count;
    struct limine_framebuffer **framebuffers;
};
struct limine_framebuffer_request { uint64_t id[4]; uint64_t revision; struct limine_framebuffer_response *response; };
struct limine_bootloader_info_response { uint64_t revision; char *name; char *version; };
struct limine_bootloader_info_request {
    uint64_t id[4]; uint64_t revision; struct limine_bootloader_info_response *response;
};
_Static_assert(sizeof(struct limine_memmap_entry) == 24, "Limine memmap ABI");
_Static_assert(sizeof(struct limine_framebuffer) == 80, "Limine framebuffer ABI");
#endif
