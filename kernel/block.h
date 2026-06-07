#pragma once
#include <stdint.h>

typedef enum {
    BLOCK_TYPE_RAMDISK = 0,
    BLOCK_TYPE_ATA,
    BLOCK_TYPE_VIRTIO,
    BLOCK_TYPE_AHCI,
    BLOCK_TYPE_UNKNOWN
} block_type_t;

typedef struct block_device {
    char name[32];
    block_type_t type;
    uint64_t block_size;   // usually 512
    uint64_t block_count;
    int (*read)(struct block_device* dev, uint64_t lba, uint64_t count, void* buf);
    int (*write)(struct block_device* dev, uint64_t lba, uint64_t count, const void* buf);
    void* private_data;    // backend specific (e.g. ramdisk buffer, ata port)
    struct block_device* next;
} block_device_t;

void block_init(void);

// Register a block device (takes ownership of the struct or copies name)
int block_register(block_device_t* dev);

// List all registered devices (for shell)
void block_list(void);

// Find by name
block_device_t* block_find(const char* name);

// Raw block helpers (for FS and shell)
int block_read(block_device_t* dev, uint64_t lba, uint64_t count, void* buf);
int block_write(block_device_t* dev, uint64_t lba, uint64_t count, const void* buf);

// Create and register a ramdisk (for tmpfs and testing)
block_device_t* block_create_ramdisk(const char* name, uint64_t size_bytes);
