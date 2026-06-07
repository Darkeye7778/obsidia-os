#include <stdint.h>
#include "drivers/framebuffer.h"
#include "console/console.h"
#include "drivers/keyboard.h"
#include "limine.h"
#include "input/line_editor.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include <stddef.h>
#include "initrd/initrd.h"
#include "vfs/vfs.h"
#include "idt.h"
#include "timer.h"
#include "paging.h"
#include "syscall.h"
#include "gdt.h"
#include "task.h"

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

// ===== SERIAL (optional debug) =====
// outb is provided by idt.h (shared low-level IO)

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

static void dummy_task(void) {
    for (;;) {
        task_yield();
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

    gdt_init();  // must be early for proper segments / TSS before IDT/user code

    if (module_request.response != NULL && module_request.response->module_count > 0) {
        struct limine_file *initrd = module_request.response->modules[0];

        initrd_set((uint64_t)initrd->address, initrd->size);
        vfs_init();
        vfs_mount_initrd_from((uint64_t)initrd->address, initrd->size);
    }

    idt_init();
    keyboard_init();
    timer_init();
    paging_init();
    syscall_init();
    tasking_init();

    // Create a simple kernel thread so 'tasks' shows something immediately (cooperative)
    extern void dummy_task(void); // defined below
    task_create_kernel_thread(dummy_task, "kernel-idle");

    console_print("This is real now.\n\n");

    console_print("Type something:\n");

    console_print("> ");
    console_set_edit_region_here();
    line_editor_init();

    console_set_edit_region_here();

    enable_interrupts();

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

