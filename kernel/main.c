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
#include "block.h"
#include "pci.h"
#include "ata.h"
#include "display.h"
#include "drivers/usb.h"
#include "drivers/net.h"
#include "drivers/audio.h"
#include "drivers/mouse.h"
#include "gui/surface.h"
#include "gui/window.h"
#include "gui/compositor.h"
#include "gui/events.h"


// ===== LIMINE FRAMEBUFFER REQUEST =====
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
volatile struct limine_module_request module_request = {
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

void serial_write(const char *str) {
    while (*str) {
        outb(0x3F8, *str++);
    }
}

static void serial_print_u32_hex(uint32_t v) {
    serial_write("0x");
    for (int i = 7; i >= 0; i--) {
        int d = (v >> (i * 4)) & 0xF;
        char c = (d < 10) ? ('0' + d) : ('A' + (d - 10));
        outb(0x3F8, c);
    }
}

static void serial_write_hex64(uint64_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_write("0x");

    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        char c[2];
        c[0] = hex[nibble];
        c[1] = '\0';
        serial_write(c);
    }
}

static void dummy_task(void) {
    for (;;) {
        task_yield();
    }
}

// Phase 3A demo GUI task: creates a window, draws, handles some keyboard for movement via events.
static void gui_demo_task(void) {
    serial_write("GUI_TASK: body entered (first time)\n");
    for (;;) {
        // Process any pending GUI events (keys posted from main input)
        gui_process_events();
        
        // Composite all windows to display
        gui_compositor_composite();
        gui_compositor_present();
        
        task_yield();  // cooperative
    }
}

static void serial_write_u64(uint64_t value) {
    char buf[21];
    int i = 20;
    buf[i] = '\0';

    if (value == 0) {
        serial_write("0");
        return;
    }

    while (value > 0 && i > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }

    serial_write(&buf[i]);
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

    display_init();  // Phase 2 abstraction layer

    // ===== INIT CONSOLE =====
    console_init(fb->width, fb->height);

    console_print("Obsidia Console Online\n");

    memory_init(memmap_request.response);

    gdt_init();  // must be early for proper segments / TSS before IDT/user code
    
    serial_write("Checking module request...\n");
    
    if (module_request.response != NULL) {
        serial_write("Module response present, count=");
        serial_write_u64(module_request.response->module_count);
        serial_write("\n");
    } else {
        serial_write("Module response NULL\n");
    }
    
    if (module_request.response != NULL && module_request.response->module_count > 0) {
        struct limine_file *initrd = module_request.response->modules[0];
        
        initrd_set((uint64_t)initrd->address, initrd->size);
        
        serial_write("Initrd module recorded for post-paging mount\n");
    } else {
        console_print("No Limine initrd module found.\n");
    }
    
    paging_init();  // enable paging early so subsequent inits (IDT, timer, tasks, GUI) run under correct tables and avoid triple fault on transition

    // Rebuild VFS metadata under the kernel-owned page tables. The initrd bytes
    // themselves were preserved during paging_init; VFS nodes are kernel heap
    // objects and should be created in the final virtual-memory environment.
    heap_init();
    vfs_init();
    if (module_request.response && module_request.response->module_count > 0) {
        struct limine_file *initrd = module_request.response->modules[0];
        if (!vfs_mount_initrd_from((uint64_t)initrd->address, initrd->size)) {
            serial_write("VFS: post-paging mount failed\n");
        }
    }
    vfs_mount_ramfs("/tmp");

    serial_write("after paging vfs_root ptr=");
    serial_write_hex64((uint64_t)vfs_get_root());
    serial_write("\n");
    
    idt_init();
    keyboard_init();
    timer_init();
    syscall_init();
    tasking_init();

    block_init();
    pci_init();
    pci_scan();  // skeleton
    ata_init();
    ata_detect_and_register();  // real storage attempt for QEMU disk

    // Phase 2 skeletons (structure only)
    usb_init();
    net_init();
    audio_init();
    mouse_init();

    // Phase 3A GUI Foundations
    gui_surface_init();
    gui_window_init();
    gui_compositor_init();
    gui_events_init();

    // Create a simple kernel thread so 'tasks' shows something immediately (cooperative)
    extern void dummy_task(void); // defined below
    task_create_kernel_thread(dummy_task, "kernel-idle");
    serial_write("MAIN: dummy task created\n");

    // Demo GUI task (creates a test window, processes events, composites)
    extern void gui_demo_task(void);
    task_create_kernel_thread(gui_demo_task, "gui-demo");
    serial_write("MAIN: gui-demo task created\n");

    console_print("This is real now.\n\n");

    console_print("Type something:\n");

    console_print("> ");
    console_set_edit_region_here();
    line_editor_init();

    console_set_edit_region_here();

    enable_interrupts();

    // Demo window using static descriptors (high-mapped kernel .bss, always reliable VA)
    // + pmm only for the pixel buffer (low phys, explicitly mapped). This avoids any late pmm struct page visibility issues for descriptors.
    serial_write("MAIN: about to create demo window\n");
    static window_t demo_win;
    static surface_t demo_surf;
    // Use a static pixel array for the demo surface content. This lives in kernel .bss (high VA,
    // reliably mapped as part of the kernel image). Avoids any late pmm low-page visibility/TLB
    // issues for the actual pixel data while still exercising the full surface + blit path.
    static uint32_t demo_pixel_buffer[316 * 176];
    for (uint64_t i=0; i < sizeof(window_t); i++) ((uint8_t*)&demo_win)[i] = 0;
    for (uint64_t i=0; i < sizeof(surface_t); i++) ((uint8_t*)&demo_surf)[i] = 0;
    demo_win.surface = &demo_surf;
    demo_win.id = 42;
    demo_win.x = 100;
    demo_win.y = 100;
    demo_win.width = 320;
    demo_win.height = 200;
    demo_win.z = 0;
    demo_win.focused = 1;
    demo_win.visible = 1;
    const char* t = "Obsidia Demo Window";
    int j = 0;
    for (; t[j] && j < 63; j++) demo_win.title[j] = t[j];
    demo_win.title[j] = 0;
    demo_win.next = 0;

    // Client content surface (compositor reserves ~20px title + 2px borders -> 316x176 client)
    uint64_t client_w = 316;
    uint64_t client_h = 176;
    demo_surf.buffer = demo_pixel_buffer;
    demo_surf.width = client_w;
    demo_surf.height = client_h;
    demo_surf.pitch = client_w;
    demo_surf.dirty = 1;
    // No pmm/mapping needed for the pixels themselves (high VA static storage is solid).
    // The surface abstraction + compositor blit of this buffer is still fully exercised.
    serial_write("MAIN: demo surf buffer (static high-VA) ready\n");

    gui_window_add_to_list(&demo_win);
    gui_window_set_focused(&demo_win);
    // re-assert after list helper
    demo_win.surface = &demo_surf;
    demo_win.visible = 1;
    demo_win.focused = 1;
    serial_write("MAIN: demo win setup done\n");

    // Fill the *client* surface entirely (no y offset) so the blitted content is obviously green
    gui_surface_clear(&demo_surf, 0xFF202020);
    gui_surface_fill_rect(&demo_surf, 0, 0, client_w, client_h, 0xFF00FF00);  // full bright green client area
    // A couple of decoration rects so it's obvious there is "content" / drawing inside the window
    gui_surface_fill_rect(&demo_surf, 10, 10, 60, 40, 0xFFFF0000);   // red box
    gui_surface_fill_rect(&demo_surf, 80, 20, 40, 30, 0xFF0000FF);   // blue box
    gui_surface_fill_rect(&demo_surf, 200, 50, 80, 60, 0xFFFFFF00);  // yellow box
    gui_surface_mark_dirty(&demo_surf);

    // Diagnostic: read back a few pixels to verify the surface buffer writes actually landed
    // (now using the static high-VA pixel array, which is reliably accessible).
    if (demo_surf.buffer) {
        uint32_t p0 = demo_surf.buffer[0];
        uint32_t p_red = demo_surf.buffer[10 + 10*client_w];   // inside red box approx
        uint32_t p_green_check = demo_surf.buffer[300];        // should still be green
        serial_write("MAIN: buf samples p0=");
        serial_print_u32_hex(p0);
        serial_write(" p_red=");
        serial_print_u32_hex(p_red);
        serial_write(" p_check=");
        serial_print_u32_hex(p_green_check);
        serial_write("\n");
    }
    serial_write("MAIN: demo window content drawn\n");
    serial_write("MAIN: demo window done\n");

    // Initial composite (compositor no longer does full clear, preserving shell text outside window area)
    serial_write("MAIN: about to initial composite\n");
    gui_compositor_composite();
    gui_compositor_present();
    serial_write("MAIN: initial composite/present done\n");

    /* Bootstrap policy now begins in a normal ELF64 ring-3 process.  The
       kernel shell remains available after init's initial child exits. */
    serial_write("MAIN: launching userspace init ELF\n");
    task_run_user_program("init.elf");

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

        if (key >= 1001 && key <= 1004) {
            // Demo hack: directly move the window on arrows and redraw.
            // This bypasses gui_post_key_event + gui_process_events (which appear to
            // trigger the exception right after printing the key for both normal keys
            // and arrows). The event system can be fixed later; this makes the arrow
            // movement actually work and keeps normal typing responsive.
            int old_x = demo_win.x;
            int old_y = demo_win.y;
            if (key == 1001) gui_window_move(&demo_win, -10, 0);
            else if (key == 1002) gui_window_move(&demo_win,  10, 0);
            else if (key == 1003) gui_window_move(&demo_win,  0, -10);
            else if (key == 1004) gui_window_move(&demo_win,  0,  10);

            // Restore the original console/shell text that was underneath the
            // window at its old position. This uses the console's internal cell
            // buffer so the text "below" the demo window is not lost when the
            // window moves. The compositor will then draw the window at the new
            // location (covering whatever is now under the new rect).
            console_refresh_rect(old_x, old_y, demo_win.width, demo_win.height);

            gui_compositor_composite();
            gui_compositor_present();
        } else {
            // Normal keys: only line editor, no GUI event posting/processing for now
            // (to prevent the exception on typing). The post/process path is the one
            // that stops right after "EVENTS: post_key_event key=..."
        }
    }
}
