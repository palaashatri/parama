#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "parama/boot_alloc.h"
static void basic_test(void) {
    struct limine_memmap_entry usable1 = { .base = 0x1003, .length = 0x3000, .type = LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry reserved = { .base = 0x9000, .length = 0x1000, .type = 1 };
    struct limine_memmap_entry usable2 = { .base = 0x10000, .length = 0x2000, .type = LIMINE_MEMMAP_USABLE };
    struct limine_memmap_entry *entries[] = { &usable1, &reserved, &usable2 };
    struct limine_memmap_response map = { .entry_count = 3, .entries = entries };
    boot_allocator_t allocator = {0};
    assert(boot_allocator_init(&allocator, &map));
    assert(allocator.free_pages == 4);  /* 0x2000,0x3000,0x10000,0x11000 */
    const uint64_t expected[] = { 0x2000, 0x3000, 0x10000, 0x11000 };
    for (unsigned i = 0; i < 4; ++i) {
        uint64_t phys = 0;
        assert(boot_alloc_frame(&allocator, &phys));
        assert(phys == expected[i]);
        assert((phys % 4096) == 0);
    }
    uint64_t addr = 5;
    assert(!boot_alloc_frame(&allocator, &addr));
    assert(addr == 5);
    assert(allocator.free_pages == 0);
}
static void error_test(void) {
    boot_allocator_t allocator = {0};
    struct limine_memmap_entry wrapped = { .base = UINT64_MAX - 0xfff, .length = 0x4000, .type = 0 };
    struct limine_memmap_entry *entries[] = { &wrapped };
    struct limine_memmap_response map = { .entry_count = 1, .entries = entries };
    assert(!boot_allocator_init(&allocator, &map));
    wrapped.base = 0x123; wrapped.length = 0x400;
    assert(!boot_allocator_init(&allocator, &map));
    assert(!boot_alloc_frame(&allocator, &(uint64_t){0}));
    assert(!boot_allocator_init(NULL, &map));
    assert(!boot_allocator_init(&allocator, NULL));
    assert(!boot_alloc_frame(NULL, &(uint64_t){0}));
    assert(!boot_alloc_frame(&allocator, NULL));
}
int main(void) {
    basic_test();
    error_test();
    puts("[UNIT][PASS] BHUMI boot frame allocator");
    return 0;
}
