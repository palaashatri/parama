#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "limine_min.h"
#include "parama/serial.h"
#include "parama/display.h"
#include "parama/boot_alloc.h"

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_start[] = LIMINE_REQUESTS_START_MARKER;
__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
__attribute__((used, section(".limine_requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST_ID, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST_ID, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID, .revision = 0
};
__attribute__((used, section(".limine_requests")))
static volatile struct limine_bootloader_info_request bootloader_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST_ID, .revision = 0
};
__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_end[] = LIMINE_REQUESTS_END_MARKER;

static boot_allocator_t boot_alloc_state;
static __attribute__((noreturn)) void stop(void) {
    for (;;) __asm__ volatile ("cli; hlt");
}
static __attribute__((noreturn)) void fatal(const char *message) {
    serial_write("[BRAHMA][FATAL] "); serial_line(message);
    stop();
}
void kmain(void) {
    serial_init();
    serial_line("[BRAHMA] PARAMA OS starting");
    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision))
        fatal("Unsupported Limine base revision");
    if (!memmap_request.response || !hhdm_request.response)
        fatal("Memory map or higher-half direct map missing");
    if (bootloader_request.response && bootloader_request.response->name) {
        serial_write("[BRAHMA] Loader: ");
        serial_line(bootloader_request.response->name);
    }
    serial_write("[BRAHMA] HHDM offset: ");
    serial_hex64(hhdm_request.response->offset); serial_write("\n");
    if (!boot_allocator_init(&boot_alloc_state, memmap_request.response))
        fatal("Bhumi boot frame allocator initialization failed");
    serial_write("[BHUMI] Usable boot frames: ");
    serial_dec64(boot_alloc_state.free_pages); serial_write("\n");
    uint64_t frame_phys = 0;
    if (!boot_alloc_frame(&boot_alloc_state, &frame_phys)) fatal("No test frame");
    if (frame_phys > UINT64_MAX - hhdm_request.response->offset)
        fatal("HHDM address overflow");
    volatile uint64_t *frame = (volatile uint64_t *)(uintptr_t)(frame_phys + hhdm_request.response->offset);
    /* Allocated USABLE pages are mapped through the bootloader HHDM. */
    for (size_t i = 0; i < 4096u / sizeof(uint64_t); ++i) frame[i] = 0;
    frame[0] = UINT64_C(0x504152414d413031); /* PARAMA01 */
    if (frame[0] != UINT64_C(0x504152414d413031)) fatal("Bhumi memory write/read failed");
    frame[0] = 0;
    serial_line("[BHUMI] Boot frame smoke test passed");
    if (framebuffer_request.response && framebuffer_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        if (boot_display_draw(fb)) serial_line("[BRAHMA] Boot diagnostic framebuffer drawn");
        else serial_line("[BRAHMA][WARN] Framebuffer mode unsupported; serial-only boot");
    } else {
        serial_line("[BRAHMA][WARN] No framebuffer from bootloader");
    }
    serial_line("[BRAHMA] BOOT_OK");
    /* This is the M1 diagnostic halt. GARUDA, SURYA, and DOOM are NOT running. */
    stop();
}
