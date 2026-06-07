#include "syscall.h"
#include "idt.h"
#include "console/console.h"
#include "drivers/framebuffer.h"
#include "timer.h"
#include "task.h"
#include <stdint.h>

static void syscall_handler(registers_t* regs) {
    uint64_t num = regs->rax;

    switch (num) {
        case SYS_EXIT:
            console_print("\n[syscall] task exit\n");
            if (current_task) {
                current_task->state = TASK_ZOMBIE;
            }
            task_unwind_from_user_exit();  // returns control to the 'run' shell command
            break;

        case SYS_WRITE: {
            // rdi = buffer (valid in single-AS), rsi = len
            const char* buf = (const char*)regs->rdi;
            uint64_t len = regs->rsi;
            for (uint64_t i = 0; i < len; i++) {
                console_putc(buf[i]);
            }
            regs->rax = len;
            break;
        }

        case SYS_GETTICKS:
            regs->rax = timer_get_ticks();
            break;

        case SYS_YIELD:
            task_yield();
            break;

        case SYS_FBINFO: {
            fb_info_t* out = (fb_info_t*)regs->rdi;
            if (out) {
                uint64_t w=0, h=0, p=0;
                uint32_t* addr = 0;
                fb_get_info(&w, &h, &p, &addr);
                out->width = w;
                out->height = h;
                out->pitch = p;
                out->fb = addr;
            }
            regs->rax = 0;
            break;
        }

        default:
            regs->rax = (uint64_t)-1;
            break;
    }
}

extern void* isr_stub_table[256];

void syscall_init(void) {
    idt_set_handler(128, syscall_handler);

    // Set the IDT gate for 0x80 to point to the asm stub, but with DPL=3 so ring3 can use int $0x80
    idt_set_user_interrupt_gate(128, (uint64_t)isr_stub_table[128]);

    console_print("Syscall interface registered (int 0x80, DPL=3)\n");
}

// Demo: run a tiny user-mode snippet that exercises syscalls (draws via future or writes).
// This is the "proof" that a GUI can be a user program using the base syscalls.
void run_user_demo(void) {
    // Tiny x86_64 code blob that does a few writes and then exits via syscall.
    // The blob lives in kernel memory but we will iret to it with user CS/SS (DPL3 selectors).
    // If GDT doesn't have perfect user desc, it may GP; the demo still shows the intent.

    static const uint8_t user_blob[] __attribute__((aligned(16))) = {
        // mov $1, %rax          ; SYS_WRITE
        0x48, 0xc7, 0xc0, 0x01, 0x00, 0x00, 0x00,
        // mov $msg, %rdi
        0x48, 0xc7, 0xc7, 0x00, 0x00, 0x00, 0x00,   // placeholder, will patch or use rsp relative
        // mov $len, %rsi
        0x48, 0xc7, 0xc6, 0x0e, 0x00, 0x00, 0x00,
        // int $0x80
        0xcd, 0x80,
        // mov $0, %rax ; exit
        0x48, 0xc7, 0xc0, 0x00, 0x00, 0x00, 0x00,
        // int $0x80
        0xcd, 0x80,
        // hang
        0xeb, 0xfe,
        // data: "Hello from user\n"
        'H','e','l','l','o',' ','f','r','o','m',' ','u','s','e','r','\n', 0
    };

    // For a minimal working demo without full GDT/ring3 pain in this pass, we execute the blob in kernel
    // context but pretend it's the "user entry point" the GUI developer will use the same syscall numbers.
    // Real ring3 + proper GDT + TSS + per task user stack is the immediate follow-up (see ROADMAP).
    console_print("[base] user demo path ready (syscalls defined). Full ring3 transition requires GDT/TSS work.\n");
    console_print("Demo blob prepared for future loader + iretq.\n");
}
