#include "idt.h"
#include "console/console.h"
#include "gdt.h"
#include "task.h"
#include <stdint.h>

extern void serial_write(const char* str);
static void serial_hex64(uint64_t v) {
    static const char h[] = "0123456789abcdef";
    for (int i = 60; i >= 0; i -= 4) outb(0x3F8, h[(v >> i) & 15]);
}

// x86_64 IDT entry
typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) idtr_t;

static idt_entry_t idt[256];
static idtr_t idtr;
static isr_handler_t handlers[256] = {0};

// CPU exception messages for common ones
static const char* exception_messages[32] = {
    "Division By Zero",
    "Debug",
    "Non Maskable Interrupt",
    "Breakpoint",
    "Into Detected Overflow",
    "Out of Bounds",
    "Invalid Opcode",
    "No Coprocessor",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Bad TSS",
    "Segment Not Present",
    "Stack Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",
    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception",
    "VMM Communication Exception",
    "Security Exception",
    "Reserved"
};

extern void* isr_stub_table[256];  // provided by isr.asm

// Port I/O
void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(0xA0, 0x20); // slave
    outb(0x20, 0x20);               // master
}

// Remap PIC to vectors 32+ so exceptions (0-31) are free
static void pic_remap(void) {
    uint8_t a1 = inb(0x21);
    uint8_t a2 = inb(0xA1);

    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20); // master offset 32
    outb(0xA1, 0x28); // slave offset 40
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);

    outb(0x21, a1);
    outb(0xA1, a2);
}

void idt_set_handler(uint8_t vector, isr_handler_t handler) {
    handlers[vector] = handler;
}

static void idt_set_gate(uint8_t vector, uint64_t handler, uint8_t type_attr, uint8_t ist) {
    idt_entry_t* e = &idt[vector];
    e->offset_low = (uint16_t)handler;
    e->selector = gdt_get_kernel_code_selector();
    e->ist = ist;
    e->type_attr = type_attr; // 0x8E = interrupt gate, present
    e->offset_mid = (uint16_t)(handler >> 16);
    e->offset_high = (uint32_t)(handler >> 32);
    e->zero = 0;
}

void idt_set_user_interrupt_gate(uint8_t vector, uint64_t handler) {
    // 0xEE = present (1), DPL=3 (11), type=interrupt gate (1110)
    idt_set_gate(vector, handler, 0xEE, 0);
}

void idt_load(void) {
    idtr.base = (uint64_t)&idt[0];
    idtr.limit = (uint16_t)(sizeof(idt_entry_t) * 256 - 1);
    __asm__ volatile ("lidt %0" : : "m"(idtr));
}

void idt_init(void) {
    // Zero IDT
    for (int i = 0; i < 256; i++) {
        // set a default handler pointing to a stub that does nothing or panic
    }

    pic_remap();

    // Install stubs from asm for 0-47 (exceptions + IRQs 0-15)
    for (uint8_t v = 0; v < 48; v++) {
        // 0x8E = 64-bit interrupt gate, present, DPL=0
        idt_set_gate(v, (uint64_t)isr_stub_table[v], 0x8E, 0);
    }
    /* A corrupt normal kernel stack must not prevent #DF diagnostics. */
    idt_set_gate(8, (uint64_t)isr_stub_table[8], 0x8E, 1);

    // Special: syscall vector 0x80 (user callable, DPL=3)
    // We will set it later when syscall is ready, or set a placeholder now.
    // For now leave 0x80 as interrupt gate from stub (will be overridden).

    idt_load();

    console_print("IDT initialized + PIC remapped\n");
}

// Default C handler called from asm stubs (registers_t defined in idt.h)
registers_t* isr_handler(registers_t* regs) {
    uint8_t vec = (uint8_t)regs->vector;

    if (vec < 32) {
        serial_write("EXCEPTION: userspace/kernel fault\n");
        outb(0x3F8, '0' + (vec / 10));
        outb(0x3F8, '0' + (vec % 10));
        serial_write(" rip="); serial_hex64(regs->rip);
        uint64_t cr2; __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));
        serial_write(" cr2="); serial_hex64(cr2);
        serial_write(" error="); serial_hex64(regs->error_code);
        serial_write(" cs="); serial_hex64(regs->cs);
        serial_write(" taskring=");
        outb(0x3F8,current_task?(char)('0'+current_task->ring):'?');
        serial_write("\n");
        if ((regs->cs & 3) == 3 && current_task && current_task->ring == 3) {
            serial_write("USER FAULT: terminating process\n");
            task_terminate_current(-((int64_t)vec), 1);
            return task_schedule_from_interrupt(regs);
        }
        // A fault whose saved CS is ring 0 is always fatal, even if it occurred
        // while servicing a userspace task.
        console_print("EXCEPTION: ");
        if (vec < 32) console_print((char*)exception_messages[vec]); else console_print("Unknown");
        console_print(" (vec=");
        // simple print vec
        char buf[4]; buf[0]='0'+(vec/10); buf[1]='0'+(vec%10); buf[2]=0;
        console_print(buf);
        console_print(")\n");
        // Halt for now (in real would kill task)
        for(;;) __asm__ volatile("hlt");
    } else if (vec >= 32 && vec < 48) {
        // IRQ
        uint8_t irq = vec - 32;
        if (handlers[vec]) {
            handlers[vec](regs);
        }
        pic_send_eoi(irq);
        if (irq == 0 && (((regs->cs & 3) == 3) ||
                         (current_task && current_task->state != TASK_RUNNING)))
            return task_schedule_from_interrupt(regs);
    } else {
        // Other (including 0x80 before syscall ready)
        if (handlers[vec]) {
            handlers[vec](regs);
        } else {
            // ignore or log
        }
    }
    if (vec == 128 && task_reschedule_requested()) {
        return task_schedule_from_interrupt(regs);
    }
    return regs;
}

void enable_interrupts(void) {
    __asm__ volatile ("sti");
}

void disable_interrupts(void) {
    __asm__ volatile ("cli");
}
