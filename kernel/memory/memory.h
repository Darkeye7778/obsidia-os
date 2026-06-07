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
