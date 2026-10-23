#ifndef PARAMA_BOOT_ALLOC_H
#define PARAMA_BOOT_ALLOC_H
#include <stdint.h>
#include <stdbool.h>
#include "limine_min.h"
/* Boot-only, monotonic, single-core frame source: not a production PMM. */
typedef struct {
    uint64_t base;
    uint64_t end;
    uint64_t cursor;
} boot_range_t;
#define BOOT_ALLOC_MAX_RANGES 128
/* State is caller-owned to support deterministic host-side unit tests. */
typedef struct {
    boot_range_t ranges[BOOT_ALLOC_MAX_RANGES];
    uint64_t count;
    uint64_t free_pages;
} boot_allocator_t;
bool boot_allocator_init(boot_allocator_t *alloc, const struct limine_memmap_response *map);
bool boot_alloc_frame(boot_allocator_t *alloc, uint64_t *physical_address);
#endif
