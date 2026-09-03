#pragma once
#include <stdint.h>

void idt_init(void);
void idt_load(void);

// Register a C handler for an interrupt vector (0-255)
typedef struct registers registers_t;
typedef void (*isr_handler_t)(registers_t* regs);
void idt_set_handler(uint8_t vector, isr_handler_t handler);

// Enable / disable IRQs (wrapper around sti/cli)
void enable_interrupts(void);
void disable_interrupts(void);

// Send EOI to PIC(s) for an IRQ (0-15)
void pic_send_eoi(uint8_t irq);

// Set a user-callable interrupt gate (DPL=3) e.g. for syscalls
void idt_set_user_interrupt_gate(uint8_t vector, uint64_t handler);

// Low level port io (used by PIC init too)
void outb(uint16_t port, uint8_t val);
uint8_t inb(uint16_t port);

// Saved register state passed to C handlers (matches push order in isr.asm)
typedef struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
} registers_t;

registers_t* isr_handler(registers_t* regs);
