#include "ata.h"
#include "block.h"
#include "console/console.h"
#include "idt.h"  // outb/inb
#include "memory/heap.h"
#include <stdint.h>

extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);

#define ATA_PRIMARY_DATA    0x1F0
#define ATA_PRIMARY_ERR     0x1F1
#define ATA_PRIMARY_SECCNT  0x1F2
#define ATA_PRIMARY_LBA0    0x1F3
#define ATA_PRIMARY_LBA1    0x1F4
#define ATA_PRIMARY_LBA2    0x1F5
#define ATA_PRIMARY_DRIVE   0x1F6
#define ATA_PRIMARY_STATUS  0x1F7
#define ATA_PRIMARY_CMD     0x1F7

static inline void ata_400ns_delay(void) {
    for (int i = 0; i < 4; i++) inb(ATA_PRIMARY_STATUS);
}

static int ata_wait_busy(void) {
    uint8_t status;
    int timeout = 100000;
    while (timeout--) {
        status = inb(ATA_PRIMARY_STATUS);
        if (!(status & 0x80)) return 0; // not busy
    }
    return -1;
}

static int ata_read_sectors(uint64_t lba, uint64_t count, void* buf) {
    if (ata_wait_busy() != 0) return -1;

    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F)); // LBA mode, master
    outb(ATA_PRIMARY_SECCNT, count & 0xFF);
    outb(ATA_PRIMARY_LBA0, lba & 0xFF);
    outb(ATA_PRIMARY_LBA1, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA2, (lba >> 16) & 0xFF);
    outb(ATA_PRIMARY_CMD, 0x20); // READ SECTORS

    uint8_t* b = (uint8_t*)buf;
    for (uint64_t s = 0; s < count; s++) {
        if (ata_wait_busy() != 0) return -1;
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & 0x01) return -1; // error
        if (!(status & 0x08)) continue; // wait DRQ

        for (int i = 0; i < 256; i++) {
            uint16_t w = (uint16_t)inb(ATA_PRIMARY_DATA) | ((uint16_t)inb(ATA_PRIMARY_DATA) << 8);
            b[s*512 + i*2 + 0] = w & 0xFF;
            b[s*512 + i*2 + 1] = (w >> 8) & 0xFF;
        }
        ata_400ns_delay();
    }
    return 0;
}

static int ata_write_sectors(uint64_t lba, uint64_t count, const void* buf) {
    // guarded for safety in phase 2
    console_print("ATA write not enabled (guarded)\n");
    (void)lba; (void)count; (void)buf;
    return -1;
}

static int ata_block_read(block_device_t* dev, uint64_t lba, uint64_t count, void* buf) {
    (void)dev;
    return ata_read_sectors(lba, count, buf);
}

static int ata_block_write(block_device_t* dev, uint64_t lba, uint64_t count, const void* buf) {
    (void)dev;
    return ata_write_sectors(lba, count, buf);
}

void ata_init(void) {
    console_print("ATA skeleton initialized\n");
}

int ata_detect_and_register(void) {
    // Simple presence check: write to drive reg, read status
    outb(ATA_PRIMARY_DRIVE, 0xA0); // master
    ata_400ns_delay();
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0xFF || status == 0x00) {
        console_print("No ATA disk detected (or floating bus)\n");
        return -1;
    }

    // Try identify or just assume present for QEMU
    block_device_t* dev = (block_device_t*)kmalloc(sizeof(block_device_t));
    if (!dev) return -1;

    int i=0; for (; "ata0"[i] && i<31; i++) dev->name[i]="ata0"[i]; dev->name[i]=0;
    dev->type = BLOCK_TYPE_ATA;
    dev->block_size = 512;
    dev->block_count = 1024 * 1024; // assume 512MB image for demo; real would IDENTIFY
    dev->read = ata_block_read;
    dev->write = ata_block_write;
    dev->private_data = 0;
    dev->next = 0;

    if (block_register(dev) == 0) {
        console_print("ATA disk registered as ata0 (PIO read supported)\n");
        return 0;
    }
    return -1;
}
