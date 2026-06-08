#pragma once
#include <stdint.h>

void gdt_init(void);

// Set the kernel stack pointer in the TSS (for privilege level changes from ring 3)
void tss_set_kernel_stack(uint64_t stack_top);

// Get selectors (for use in iret/syscall entry if needed)
uint16_t gdt_get_kernel_code_selector(void);
uint16_t gdt_get_kernel_data_selector(void);
uint16_t gdt_get_user_code_selector(void);
uint16_t gdt_get_user_data_selector(void);
uint16_t gdt_get_tss_selector(void);

// Reload GDT and segments under the current page tables (safe to call after CR3 switch)
void gdt_reload(void);
