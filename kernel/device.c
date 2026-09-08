#include "device.h"
#include "block.h"
#include "partition.h"
#include "pci.h"
#include "ata.h"
#include "ahci.h"
#include "drivers/usb.h"
#include "drivers/net.h"
#include "drivers/audio.h"
#include "drivers/mouse.h"
#include "dma.h"

extern void serial_write(const char*);

/* Platform discovery owns initialization order. Drivers register their device
   objects with their subsystem; policy and userspace service startup do not
   belong here. */
void platform_devices_init(void) {
    block_init();
    serial_write(dma_self_test()==0?"DMA: contiguous allocation self-test passed\n":"DMA: allocation self-test FAILED\n");
    serial_write(block_partition_self_test()==0?"PARTITION: MBR/GPT and volume I/O self-test passed\n":"PARTITION: self-test FAILED\n");
    pci_init();
    pci_scan();
    ahci_detect_and_register();
    ata_init();
    ata_detect_and_register();
    usb_init();
    net_init();
    audio_init();
    mouse_init();
}
