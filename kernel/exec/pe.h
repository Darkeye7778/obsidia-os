#pragma once

#include <stdint.h>
#include "../vfs/vfs.h"

#define PE_MACHINE_I386  0x014cU
#define PE_MACHINE_AMD64 0x8664U
#define PE_MAGIC_PE32    0x010bU
#define PE_MAGIC_PE32_PLUS 0x020bU
#define PE_MAX_SECTIONS 96U

typedef struct {
    uint16_t machine;
    uint16_t optional_magic;
    uint16_t section_count;
    uint64_t image_base;
    uint32_t image_size;
    uint32_t headers_size;
    uint32_t entry_rva;
    uint32_t section_table_offset;
    uint32_t import_rva;
    uint32_t import_size;
    uint32_t relocation_rva;
    uint32_t relocation_size;
} pe_image_info_t;

typedef struct {
    uint8_t name[8];
    uint32_t virtual_size;
    uint32_t virtual_address;
    uint32_t raw_size;
    uint32_t raw_offset;
    uint32_t characteristics;
} pe_section_info_t;

int pe_inspect(vfs_node_t* node, pe_image_info_t* info,
               pe_section_info_t* sections, uint32_t section_capacity);
int pe_load_process(vfs_node_t* node, uint64_t* cr3_out,
                    uint64_t* entry_out, uint64_t* stack_top_out);
