#include "device.h"
#include "block.h"
#include "pci.h"
#include "ata.h"
#include "drivers/usb.h"
#include "drivers/net.h"
#include "drivers/audio.h"
#include "drivers/mouse.h"

/* Platform discovery owns initialization order. Drivers register their device
   objects with their subsystem; policy and userspace service startup do not
   belong here. */
void platform_devices_init(void) {
    block_init();
    pci_init();
    pci_scan();
    ata_init();
    ata_detect_and_register();
    usb_init();
    net_init();
    audio_init();
    mouse_init();
}
