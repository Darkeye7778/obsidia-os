#pragma once
#include <stdint.h>

void paging_init(void);

// Map a single 4KiB page (physical -> virtual). For future use by loader/GUI.
int paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags);

// Get the physical address of the current PML4 (for CR3).
uint64_t paging_get_cr3(void);
