#include "parama/boot_alloc.h"
#include <stddef.h>
#define PAGE_SIZE UINT64_C(4096)
static uint64_t ceil_page(uint64_t n) {
    /* Reject values that would overflow when rounded. */
    if (n > UINT64_MAX - (PAGE_SIZE - 1)) return UINT64_MAX;
    return (n + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}
bool boot_allocator_init(boot_allocator_t *alloc, const struct limine_memmap_response *map) {
    if (!alloc || !map || !map->entries || map->entry_count > 65536) return false;
    alloc->count = 0;
    alloc->free_pages = 0;
    for (uint64_t i = 0; i < map->entry_count; ++i) {
        const struct limine_memmap_entry *e = map->entries[i];
        if (!e || e->type != LIMINE_MEMMAP_USABLE || e->length == 0) continue;
        if (e->length > UINT64_MAX - e->base) return false;
        uint64_t start = ceil_page(e->base);
        uint64_t end = (e->base + e->length) & ~(PAGE_SIZE - 1);
        if (start >= end) continue;
        if (alloc->count == BOOT_ALLOC_MAX_RANGES) return false;
        uint64_t pages = (end - start) / PAGE_SIZE;
        if (pages > UINT64_MAX - alloc->free_pages) return false;
        alloc->ranges[alloc->count++] = (boot_range_t){ start, end, start };
        alloc->free_pages += pages;
    }
    return alloc->free_pages > 0;
}
bool boot_alloc_frame(boot_allocator_t *alloc, uint64_t *physical_address) {
    if (!alloc || !physical_address) return false;
    for (uint64_t i = 0; i < alloc->count; ++i) {
        boot_range_t *r = &alloc->ranges[i];
        if (r->cursor < r->end) {
            *physical_address = r->cursor;
            r->cursor += PAGE_SIZE;
            --alloc->free_pages;
            return true;
        }
    }
    return false;
}
