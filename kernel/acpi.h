#pragma once
#include <stdint.h>
#include "limine.h"

#define ACPI_MAX_IOAPICS 4
#define ACPI_MAX_INTERRUPT_OVERRIDES 16

typedef struct {
    uint8_t id;
    uint32_t address;
    uint32_t gsi_base;
} acpi_ioapic_t;

typedef struct {
    uint8_t source_irq;
    uint32_t gsi;
    uint16_t flags;
} acpi_interrupt_override_t;

typedef struct {
    uint8_t revision;
    uint8_t has_fadt;
    uint8_t has_s5;
    uint8_t has_reset;
    uint16_t pm1a_control;
    uint16_t pm1b_control;
    uint8_t sleep_type_a;
    uint8_t sleep_type_b;
    uint8_t has_madt;
    uint8_t ioapic_count;
    uint8_t interrupt_override_count;
    uint64_t local_apic_address;
    acpi_ioapic_t ioapics[ACPI_MAX_IOAPICS];
    acpi_interrupt_override_t interrupt_overrides[ACPI_MAX_INTERRUPT_OVERRIDES];
} acpi_platform_info_t;

/* Firmware discovery runs while Limine's mappings are still active.  Only
   validated, bounded control metadata is retained afterward. */
int acpi_init(void* rsdp_address,uint64_t hhdm_offset,struct limine_memmap_response* memory_map);
const acpi_platform_info_t* acpi_platform_info(void);
int acpi_power_off(void);
int acpi_reboot(void);
