#include "memory.h"
#include "../console/console.h"

static struct limine_memmap_response* g_memmap = 0;
static uint64_t total_usable_memory = 0;

static void print_hex_digit(uint8_t value) {
    char c = value < 10 ? '0' + value : 'A' + (value - 10);
    console_putc(c);
}

static void print_hex64(uint64_t value) {
    console_print("0x");

    for (int i = 15; i >= 0; i--) {
        uint8_t digit = (value >> (i * 4)) & 0xF;
        print_hex_digit(digit);
    }
}

static void print_dec(uint64_t value) {
    char buffer[32];
    int i = 0;

    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value > 0 && i < 31) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        console_putc(buffer[--i]);
    }
}

static const char* memmap_type_name(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:
            return "USABLE";
        case LIMINE_MEMMAP_RESERVED:
            return "RESERVED";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI_RECLAIM";
        case LIMINE_MEMMAP_ACPI_NVS:
            return "ACPI_NVS";
        case LIMINE_MEMMAP_BAD_MEMORY:
            return "BAD";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "BOOTLOADER";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            return "KERNEL";
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return "FRAMEBUFFER";
        default:
            return "UNKNOWN";
    }
}

void memory_init(struct limine_memmap_response* memmap) {
    g_memmap = memmap;
    total_usable_memory = 0;

    if (!g_memmap) {
        return;
    }

    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_usable_memory += entry->length;
        }
    }
}

uint64_t memory_get_total_usable(void) {
    return total_usable_memory;
}

void memory_print_map(void) {
    if (!g_memmap) {
        console_print("No memory map available.\n");
        return;
    }

    console_print("Memory Map:\n");

    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

        console_print("  ");
        console_print(memmap_type_name(entry->type));
        console_print(" base=");
        print_hex64(entry->base);
        console_print(" length=");
        print_hex64(entry->length);
        console_print("\n");
    }

    console_print("Total usable memory: ");
    print_dec(total_usable_memory / 1024 / 1024);
    console_print(" MiB\n");
}
