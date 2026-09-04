#pragma once
#include <stdint.h>
#define PAGING_SHARED (1ULL<<9)

void paging_init(void);
uint64_t paging_create_user_address_space(void);
void paging_destroy_user_address_space(uint64_t cr3);
void paging_activate(uint64_t cr3);
uint64_t paging_get_kernel_cr3(void);

// Map a single 4KiB page (physical -> virtual). For future use by loader/GUI.
int paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags);
int paging_map_page_in(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags);
int paging_user_range_valid(uint64_t cr3, uint64_t addr, uint64_t size, int write_access);
uint64_t paging_translate_in(uint64_t cr3, uint64_t virt, uint64_t* flags);
uint64_t paging_unmap_page_in(uint64_t cr3, uint64_t virt, uint64_t* flags);

// Get the physical address of the current PML4 (for CR3).
uint64_t paging_get_cr3(void);

// Preserve a virtual range by re-establishing the same virt->phys mapping from the
// *current* (pre-switch) page tables into our new tables. Used to keep Limine
// response/module pointers and data blobs accessible after CR3 switch.
int paging_preserve_range(uint64_t vaddr, uint64_t size);

// Preserve mappings for the module_request response + all loaded module file
// data ranges so that post-paging accesses to module_request and initrd content
// do not fault.
void paging_preserve_limine_modules(void);

// Invalidate TLB entry for a page (to ensure writes to a late-allocated pmm page
// are visible after identity mapping).
void paging_invlpg(uint64_t addr);
