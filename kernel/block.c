#include "block.h"
#include "console/console.h"
#include "memory/heap.h"
#include "memory/memory.h"
#include <stddef.h>
#include <stdint.h>

static block_device_t* block_devices = 0;
static int block_count = 0;

// ===== RAMDISK backend (first usable storage for testing / ramfs) =====

typedef struct {
    uint8_t* data;
    uint64_t size;
} ramdisk_priv_t;

static int ramdisk_read(block_device_t* dev, uint64_t lba, uint64_t count, void* buf) {
    ramdisk_priv_t* priv = (ramdisk_priv_t*)dev->private_data;
    uint64_t byte_off = lba * dev->block_size;
    uint64_t byte_len = count * dev->block_size;
    if (byte_off + byte_len > priv->size) return -1;
    for (uint64_t i = 0; i < byte_len; i++) {
        ((uint8_t*)buf)[i] = priv->data[byte_off + i];
    }
    return 0;
}

static int ramdisk_write(block_device_t* dev, uint64_t lba, uint64_t count, const void* buf) {
    ramdisk_priv_t* priv = (ramdisk_priv_t*)dev->private_data;
    uint64_t byte_off = lba * dev->block_size;
    uint64_t byte_len = count * dev->block_size;
    if (byte_off + byte_len > priv->size) return -1;
    for (uint64_t i = 0; i < byte_len; i++) {
        priv->data[byte_off + i] = ((const uint8_t*)buf)[i];
    }
    return 0;
}

block_device_t* block_create_ramdisk(const char* name, uint64_t size_bytes) {
    if (size_bytes == 0 || (size_bytes % 512) != 0) return 0;

    block_device_t* dev = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!dev) return 0;

    ramdisk_priv_t* priv = (ramdisk_priv_t*)kmalloc(sizeof(ramdisk_priv_t));
    if (!priv) {
        return 0;
    }

    priv->data = (uint8_t*)kmalloc(size_bytes);
    if (!priv->data) return 0;
    priv->size = size_bytes;

    // zero the disk
    for (uint64_t i = 0; i < size_bytes; i++) priv->data[i] = 0;

    // fill dev
    int i = 0;
    for (; name[i] && i < 31; i++) dev->name[i] = name[i];
    dev->name[i] = 0;
    dev->type = BLOCK_TYPE_RAMDISK;
    dev->block_size = 512;
    dev->block_count = size_bytes / 512;
    dev->read = ramdisk_read;
    dev->write = ramdisk_write;
    dev->private_data = priv;
    dev->next = 0;

    if (block_register(dev) != 0) return 0;
    return dev;
}

void block_init(void) {
    block_devices = 0;
    block_count = 0;
    console_print("Block device layer initialized\n");

    // Create a default ramdisk for tmpfs / testing (4 MiB)
    block_create_ramdisk("ram0", 4 * 1024 * 1024);
}

int block_register(block_device_t* dev) {
    if (!dev || !dev->name[0] || !dev->read) return -1;

    dev->next = block_devices;
    block_devices = dev;
    block_count++;
    console_print("Block device registered: ");
    console_print(dev->name);
    console_print("\n");
    return 0;
}

void block_list(void) {
    if (!block_devices) {
        console_print("No block devices registered.\n");
        return;
    }
    console_print("Block devices:\n");
    block_device_t* d = block_devices;
    while (d) {
        console_print("  ");
        console_print(d->name);
        console_print(" type=");
        switch (d->type) {
            case BLOCK_TYPE_RAMDISK: console_print("ramdisk"); break;
            case BLOCK_TYPE_ATA: console_print("ata"); break;
            case BLOCK_TYPE_VIRTIO: console_print("virtio"); break;
            default: console_print("unknown"); break;
        }
        console_print(" blksz=");
        // simple print
        char numbuf[32];
        // reuse memory_print_dec if possible, but for now basic
        uint64_t bs = d->block_size;
        int i = 0;
        if (bs == 0) { numbuf[i++]='0'; }
        while (bs > 0 && i < 30) { numbuf[i++] = '0' + (bs % 10); bs /= 10; }
        while (i > 0) console_putc(numbuf[--i]);
        console_print(" blocks=");
        uint64_t bc = d->block_count;
        i = 0;
        if (bc == 0) { numbuf[i++]='0'; }
        while (bc > 0 && i < 30) { numbuf[i++] = '0' + (bc % 10); bc /= 10; }
        while (i > 0) console_putc(numbuf[--i]);
        console_print("\n");
        d = d->next;
    }
}

block_device_t* block_find(const char* name) {
    block_device_t* d = block_devices;
    while (d) {
        // strcmp
        const char* a = d->name;
        const char* b = name;
        int match = 1;
        while (*a && *b) { if (*a++ != *b++) { match=0; break; } }
        if (match && *a == 0 && *b == 0) return d;
        d = d->next;
    }
    return 0;
}

int block_read(block_device_t* dev, uint64_t lba, uint64_t count, void* buf) {
    if (!dev || !dev->read) return -1;
    return dev->read(dev, lba, count, buf);
}

int block_write(block_device_t* dev, uint64_t lba, uint64_t count, const void* buf) {
    if (!dev || !dev->write) return -1;
    return dev->write(dev, lba, count, buf);
}

