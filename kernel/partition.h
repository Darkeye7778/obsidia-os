#pragma once
#include <stdint.h>
#include "block.h"

#define BLOCK_PARTITION_LABEL_MAX 36U
#define BLOCK_PARTITION_SCHEME_MBR 1U
#define BLOCK_PARTITION_SCHEME_GPT 2U
extern const uint8_t OBSIDIA_STATE_PARTITION_TYPE_GUID[16];

typedef struct {
    uint32_t scheme;
    uint32_t index;
    uint64_t start_lba;
    uint64_t block_count;
    uint8_t mbr_type;
    uint8_t type_guid[16];
    uint8_t unique_guid[16];
    char label[BLOCK_PARTITION_LABEL_MAX];
} block_partition_info_t;

/* Validates a disk partition table and registers bounded child block devices.
   Returns zero when no table is present, a positive partition count for a
   recognized table, and a negative value for malformed or unreadable media. */
int block_scan_partitions(block_device_t* disk);
int block_partition_info(block_device_t* device,block_partition_info_t* result);
block_device_t* block_find_partition_by_type(block_device_t* parent,const uint8_t type_guid[16]);

/* Deterministic parser and translated-I/O regression over the bootstrap ramdisk. */
int block_partition_self_test(void);
