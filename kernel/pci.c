#include "pci.h"
#include "console/console.h"
#include "idt.h"  // for outb/inb if not global, but we have them via block or idt
#include <stdint.h>

// Use inb/outb from idt (they are global)
extern void outb(uint16_t port, uint8_t val);
extern uint8_t inb(uint16_t port);

void pci_init(void) {
    console_print("PCI skeleton initialized\n");
}

uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    // Create configuration address
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    outb(0xCF8, (address >> 0) & 0xFF);  // note: full 32 bit write needed but simplified for hobby
    // Better full write:
    // Actually for simplicity use full 32bit out to 0xCF8
    // Use asm for 32bit
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(0xCF8));
    uint32_t tmp;
    __asm__ volatile ("inl %1, %0" : "=a"(tmp) : "Nd"(0xCFC));
    return tmp;
}

void pci_get_class(uint8_t bus, uint8_t slot, uint8_t func, uint8_t* class_code, uint8_t* subclass) {
    uint32_t reg = pci_config_read32(bus, slot, func, 0x08);
    *class_code = (reg >> 24) & 0xFF;
    *subclass = (reg >> 16) & 0xFF;
}

void pci_scan(void) {
    console_print("PCI scan (skeleton):\n");
    for (uint16_t bus = 0; bus < 1; bus++) {  // limit to bus 0 for speed/safety
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_config_read32(bus, slot, func, 0x00);
                if ((id & 0xFFFF) != 0xFFFF) {  // valid device
                    uint8_t classc, sub;
                    pci_get_class(bus, slot, func, &classc, &sub);
                    console_print("  found dev at ");
                    // simple hex print not full, but note
                    console_print("bus/slot/func (class/sub) ");
                    // for demo just count or print basic
                    // To keep simple, print when mass storage or network found
                    if (classc == 0x01) { // mass storage
                        console_print("MASS STORAGE ");
                    } else if (classc == 0x02) {
                        console_print("NET ");
                    }
                    console_print("\n");
                }
            }
        }
    }
    console_print("PCI scan complete (limited).\n");
}
