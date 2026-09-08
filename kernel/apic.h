#pragma once
#include <stdint.h>

/* Initializes xAPIC/IOAPIC routing from validated ACPI MADT data. Failure is
   non-fatal: callers continue through the legacy 8259 PIC implementation. */
int apic_init(void);
int apic_is_active(void);
int apic_route_legacy_irq(uint8_t irq,uint8_t vector);
int apic_route_pci_irq(uint8_t irq,uint8_t vector);
void apic_send_eoi(void);
uint8_t apic_boot_processor_id(void);
