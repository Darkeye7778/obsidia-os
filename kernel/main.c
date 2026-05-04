#include <stdint.h>
#include "limine.h"
#include "drivers/framebuffer.h"

// ===== LIMINE REQUESTS =====

__attribute__((used, section(".limine_requests")))
static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests_start")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests_end")))
static volatile LIMINE_REQUESTS_END_MARKER;

// ===== SERIAL (for debug) =====

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static void serial_init(void) {
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x80);
    outb(0x3F8 + 0, 0x03);
    outb(0x3F8 + 1, 0x00);
    outb(0x3F8 + 3, 0x03);
    outb(0x3F8 + 2, 0xC7);
    outb(0x3F8 + 4, 0x0B);
}

static void serial_write(const char *str) {
    while (*str) {
        outb(0x3F8, *str++);
    }
}

// ===== KERNEL ENTRY =====

void kmain(void) {
    serial_init();
    serial_write("Starting framebuffer...\n");

    if (!framebuffer_request.response ||
        framebuffer_request.response->framebuffer_count < 1) {
        serial_write("NO FRAMEBUFFER\n");
        while (1) __asm__ volatile ("hlt");
    }

    struct limine_framebuffer *fb =
        framebuffer_request.response->framebuffers[0];

    // 🎨 Draw color bars
    fb_init((uint32_t*)fb->address, fb->width, fb->height, fb->pitch);

    fb_clear(0x00202020);

    fb_fill_rect(100, 100, 300, 200, 0x00FF0000);
    fb_fill_rect(500, 100, 300, 200, 0x0000FF00);
    fb_fill_rect(900, 100, 300, 200, 0x000000FF);
    serial_write("Framebuffer OK\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
