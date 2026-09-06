#pragma once

#include <stdint.h>
#include "../vfs/vfs.h"

#define OBSX_VERSION 1U
#define OBSX_ARCH_X86_64 0x8664U
#define OBSX_MAX_SEGMENTS 64U

typedef struct __attribute__((packed)) {
    uint8_t magic[8];
    uint16_t version;
    uint16_t header_size;
    uint32_t architecture;
    uint32_t flags;
    uint32_t segment_count;
    uint32_t segment_entry_size;
    uint64_t entry_point;
    uint64_t segment_table_offset;
    uint64_t import_table_offset;
    uint64_t import_table_size;
    uint64_t reserved;
} obsx_header_t;

typedef struct __attribute__((packed)) {
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t flags;
    uint32_t alignment;
    uint64_t reserved;
} obsx_segment_t;

int obsx_probe(vfs_node_t* node);
int obsx_load_process(vfs_node_t* node, uint64_t* cr3_out,
                      uint64_t* entry_out, uint64_t* stack_top_out);
