#include <stdint.h>
#include "limine.h"

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

    uint32_t *pixels = fb->address;
    uint64_t width = fb->width;
    uint64_t height = fb->height;
    uint64_t pitch = fb->pitch / 4;

    // 🎨 Draw color bars
    for (uint64_t y = 0; y < height; y++) {
        for (uint64_t x = 0; x < width; x++) {
            if (x < width / 3) {
                pixels[y * pitch + x] = 0x00FF0000; // red
            } else if (x < (width * 2) / 3) {
                pixels[y * pitch + x] = 0x0000FF00; // green
            } else {
                pixels[y * pitch + x] = 0x000000FF; // blue
            }
        }
    }

    serial_write("Framebuffer OK\n");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
