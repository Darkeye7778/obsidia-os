#pragma once
#include <stdint.h>
#include "../limine.h"

void memory_init(struct limine_memmap_response* memmap);
void memory_print_map(void);
uint64_t memory_get_total_usable(void);
