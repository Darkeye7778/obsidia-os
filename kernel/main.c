#include <stdint.h>
#include "drivers/framebuffer.h"
#include "console/console.h"
#include "drivers/keyboard.h"
#include "limine.h"
#include "input/line_editor.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include <stddef.h>

// ===== LIMINE FRAMEBUFFER REQUEST =====
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

// ===== SERIAL (optional debug, you already had this) =====
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
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
    serial_write("Starting kernel...\n");

    // ===== FRAMEBUFFER CHECK =====
    if (!framebuffer_request.response ||
        framebuffer_request.response->framebuffer_count < 1) {
        serial_write("NO FRAMEBUFFER\n");
        while (1) __asm__ volatile ("hlt");
    }

    __attribute__((used, section(".limine_requests")))
    static volatile struct limine_memmap_request memmap_request = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0
    };

    struct limine_framebuffer *fb =
        framebuffer_request.response->framebuffers[0];

    // ===== INIT FRAMEBUFFER =====
    fb_init((uint32_t*)fb->address, fb->width, fb->height, fb->pitch);

    // ===== CLEAR SCREEN =====
    fb_clear(0x00202020);

    // ===== INIT CONSOLE =====
    console_init(fb->width, fb->height);

    console_print("Obsidia Console Online\n");

    memory_init(memmap_request.response);
    heap_init();

    if (module_request.response == NULL || module_request.response->module_count == 0) {
        console_print("No initrd loaded.\n");
    } else {
        struct limine_file *initrd = module_request.response->modules[0];

        console_print("Initrd loaded.\n");
        console_print("Address: ");
        memory_print_hex64((uint64_t)initrd->address);
        console_print("\nSize: ");
        memory_print_dec(initrd->size);
        console_print(" bytes\n");
    }

/*
    memory_print_map();

    void* test_page = pmm_alloc_page();

    console_print("Allocated page at: ");
    memory_print_hex64((uint64_t)test_page);
    console_print("\n");

    pmm_free_page(test_page);

    console_print("Freed that page.\n");

    void* test_page2 = pmm_alloc_page();

    console_print("Allocated again at: ");
    memory_print_hex64((uint64_t)test_page2);
    console_print("\n");

    void* heap_test = kmalloc(64);
    console_print("kmalloc test: ");
    memory_print_hex64((uint64_t)heap_test);
    console_print("\n");
*/
    console_print("This is real now.\n\n");

    console_print("Type something:\n");

    console_print("> ");
    console_set_edit_region_here();
    line_editor_init();

    console_set_edit_region_here();

    // ===== MAIN INPUT LOOP =====
    while (1) {
        int key = keyboard_getkey();

        if (key == KEY_CTRL_L) {
            fb_clear(0x00202020);
            console_init(fb->width, fb->height);
            console_print("Obsidia Console Online\n");
            console_print("Screen cleared.\n");
            console_print("> ");
            console_set_edit_region_here();
            line_editor_init();
            continue;
        }

        line_editor_handle_key(key);
    }
}

