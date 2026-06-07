#pragma once
#include <stdint.h>

void initrd_set(uint64_t address, uint64_t size);
void initrd_print_info();
void initrd_list_files();
void initrd_cat_file(const char* filename);

// Raw access for VFS backend (the module memory stays valid)
uint64_t initrd_get_raw_addr(void);
uint64_t initrd_get_raw_size(void);

