#pragma once
#include <stdint.h>

typedef struct {
    uint8_t bus,slot,function;
    uint16_t vendor_id,device_id;
    uint8_t class_code,subclass,programming_interface,header_type;
} pci_device_t;

void pci_init(void);

// Read 32-bit from PCI config space
uint32_t pci_config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
void pci_config_write32(uint8_t bus,uint8_t slot,uint8_t func,uint8_t offset,uint32_t value);
int pci_find_class(uint8_t class_code,uint8_t subclass,uint8_t programming_interface,pci_device_t*result);
int pci_enable_memory_bus_master(const pci_device_t*device);
int pci_bar_memory_address(const pci_device_t*device,uint8_t bar,uint64_t*address);
int pci_legacy_interrupt(const pci_device_t*device,uint8_t*irq_line,uint8_t*interrupt_pin);
int pci_find_capability(const pci_device_t*device,uint8_t capability_id,uint8_t*offset);
int pci_enable_msi(const pci_device_t*device,uint8_t vector,uint8_t destination_apic_id);

// Simple device scan (prints found devices for now)
void pci_scan(void);

// Get class/subclass for a device
void pci_get_class(uint8_t bus, uint8_t slot, uint8_t func, uint8_t* class_code, uint8_t* subclass);
