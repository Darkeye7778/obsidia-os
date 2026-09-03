#pragma once
#include <stdint.h>
#include "vfs/vfs.h"

int elf_load_process(vfs_node_t* node, uint64_t* cr3_out,
                     uint64_t* entry_out, uint64_t* stack_top_out);
