#pragma once
#include <stdint.h>

void pci_init(void);

// Read 32-bit from PCI config space
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

// Simple device scan (prints found devices for now)
void pci_scan(void);

// Get class/subclass for a device
void pci_get_class(uint8_t bus, uint8_t slot, uint8_t func, uint8_t* class_code, uint8_t* subclass);
