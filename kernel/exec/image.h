#pragma once

#include <stdint.h>
#include "../vfs/vfs.h"

/* Stable values shared with the public userspace application API. */
typedef enum {
    EXEC_FORMAT_UNKNOWN = 0,
    EXEC_FORMAT_ELF64 = 1,
    EXEC_FORMAT_OBSIDIA_NATIVE = 2,
    EXEC_FORMAT_PE32 = 3,
    EXEC_FORMAT_PE32_PLUS = 4
} exec_format_t;

typedef struct {
    uint64_t file_offset;
    uint64_t virtual_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint32_t flags;
} exec_segment_t;

#define EXEC_SEGMENT_READ  1U
#define EXEC_SEGMENT_WRITE 2U
#define EXEC_SEGMENT_EXEC  4U

exec_format_t exec_detect(vfs_node_t* node);
const char* exec_format_name(exec_format_t format);
int exec_load(vfs_node_t* node, exec_format_t format, uint64_t* cr3_out,
              uint64_t* entry_out, uint64_t* stack_top_out);

/* General-purpose mapper used by independently parsed executable formats. */
int exec_map_image(vfs_node_t* node, const exec_segment_t* segments,
                   uint32_t segment_count, uint64_t entry,
                   uint64_t* cr3_out, uint64_t* stack_top_out);
