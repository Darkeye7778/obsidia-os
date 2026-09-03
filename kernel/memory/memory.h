#pragma once
#include <stdint.h>
#include "../limine.h"

void memory_init(struct limine_memmap_response* memmap);
void memory_print_map();
uint64_t memory_get_total_usable();
uint64_t memory_get_total_pages();
uint64_t memory_get_usable_pages();
void* pmm_alloc_page();
void pmm_free_page(void* addr);
void memory_print_hex64(uint64_t value);
void memory_print_dec(uint64_t value);
uint64_t memory_get_free_pages(void);
uint64_t memory_get_max_physical(void);

// Allocate 'count' consecutive pages (returns base physical address, or NULL if not possible).
// Useful for things that need contiguous kernel stacks etc. (identity mapped).
void* pmm_alloc_pages(uint64_t count);
void pmm_free_pages(void* addr, uint64_t count);
