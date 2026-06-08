#include "gdt.h"
#include "memory/heap.h"  // for stack alloc if needed
#include "memory/memory.h"
#include "console/console.h"
#include <stdint.h>

#define GDT_ENTRIES 7  // null + kcode + kdata + ucode + udata + tss (2 slots)

// Standard GDT entry (8 bytes)
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

// 64-bit TSS descriptor is 16 bytes
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) tss_entry_t;

// TSS structure (minimal, we only really need rsp0 for syscalls/interrupts from user)
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];
    uint32_t reserved2;
    uint32_t reserved3;
    uint16_t reserved4;
    uint16_t iomap_base;
} __attribute__((packed)) tss_t;

static gdt_entry_t gdt[GDT_ENTRIES];
static tss_entry_t* tss_gdt_entry; // points into gdt for the tss slots
static tss_t tss;
static uint64_t tss_kernel_stack[4096 / sizeof(uint64_t)]; // 4KiB kernel stack for TSS rsp0 (small for base)

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) gdtr_t;

static gdtr_t gdtr;

// Helper to set a standard segment descriptor
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[idx].base_low    = (base & 0xFFFF);
    gdt[idx].base_middle = (base >> 16) & 0xFF;
    gdt[idx].base_high   = (base >> 24) & 0xFF;

    gdt[idx].limit_low   = (limit & 0xFFFF);
    gdt[idx].granularity = (limit >> 16) & 0x0F;

    gdt[idx].granularity |= gran & 0xF0;
    gdt[idx].access       = access;
}

// Set 64-bit TSS descriptor (takes two GDT slots)
static void gdt_set_tss(int idx, uint64_t base, uint32_t limit) {
    gdt_entry_t* e = &gdt[idx];
    tss_entry_t* te = (tss_entry_t*)e;

    te->limit_low    = limit & 0xFFFF;
    te->base_low     = base & 0xFFFF;
    te->base_mid     = (base >> 16) & 0xFF;
    te->access       = 0x89;  // present, 64bit TSS, DPL=0
    te->granularity  = (limit >> 16) & 0x0F;
    te->base_high    = (base >> 24) & 0xFF;
    te->base_upper   = (base >> 32) & 0xFFFFFFFF;
    te->reserved     = 0;
}

void gdt_init(void) {
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // Kernel code segment (ring 0, 64-bit)
    // access: P=1, DPL=0, S=1, type=0xA (code, read, accessed)
    // gran: G=1 (4K), L=1 (64-bit), D=0
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);

    // Kernel data segment (ring 0)
    // access: P=1 DPL=0 S=1 type=0x2 (data, write, accessed)
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);

    // User code segment (ring 3, 64-bit)
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);

    // User data segment (ring 3)
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xA0);

    // TSS (starts at index 5, uses slots 5 and 6)
    uint64_t tss_addr = (uint64_t)&tss;
    gdt_set_tss(5, tss_addr, sizeof(tss) - 1);

    // Clear TSS
    for (int i = 0; i < (int)sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;
    tss.iomap_base = sizeof(tss);  // no iomap

    // Initial kernel stack for TSS (top of our small stack)
    tss.rsp0 = (uint64_t)&tss_kernel_stack[4096 / sizeof(uint64_t)];

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint64_t)&gdt[0];

    // Load GDT
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));

    // Reload data segments (CS reload via far return or jmp)
    __asm__ volatile (
        "mov $0x10, %%ax\n"   // kernel data
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        ::: "ax", "memory"
    );

    // Reload CS with a far jump (using a label)
    __asm__ volatile (
        "push $0x08\n"        // kernel code selector
        "push $1f\n"
        "lretq\n"
        "1:\n"
        ::: "memory"
    );

    // Load TSS (selector 0x28 = 5*8)
    __asm__ volatile ("ltr %0" : : "r"((uint16_t)0x28));

    console_print("GDT + TSS initialized (kernel/user segments + TSS rsp0)\n");
}

void tss_set_kernel_stack(uint64_t stack_top) {
    tss.rsp0 = stack_top;
}

uint16_t gdt_get_kernel_code_selector(void) { return 0x08; }
uint16_t gdt_get_kernel_data_selector(void) { return 0x10; }
uint16_t gdt_get_user_code_selector(void)   { return 0x1B; }  // 0x18 | 3
uint16_t gdt_get_user_data_selector(void)   { return 0x23; }  // 0x20 | 3
uint16_t gdt_get_tss_selector(void)         { return 0x28; }

void gdt_reload(void) {
    __asm__ volatile ("lgdt %0" : : "m"(gdtr));

    // reload data segments
    __asm__ volatile (
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        ::: "ax", "memory"
    );

    // reload CS
    __asm__ volatile (
        "push $0x08\n"
        "push $1f\n"
        "lretq\n"
        "1:\n"
        ::: "memory"
    );
}
