#include "memory.h"
#include "../console/console.h"

#define PAGE_SIZE 4096

static uint64_t total_pages = 0;
static uint64_t usable_pages = 0;

static struct limine_memmap_response* g_memmap = 0;
static uint64_t total_usable_memory = 0;

static uint8_t* pmm_bitmap = 0;
static uint64_t pmm_bitmap_size = 0;

static uint64_t pmm_max_page = 0;

static void bitmap_set(uint64_t page) {
    pmm_bitmap[page / 8] |= (1 << (page % 8));
}

static void bitmap_clear(uint64_t page) {
    pmm_bitmap[page / 8] &= ~(1 << (page % 8));
}

static int bitmap_test(uint64_t page) {
    return pmm_bitmap[page / 8] & (1 << (page % 8));
}

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

void memory_print_hex64(uint64_t value) {
    print_hex64(value);
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

void memory_print_dec(uint64_t value) {
    print_dec(value);
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

    total_pages = 0;
    usable_pages = 0;

    if (!g_memmap) {
        return;
    }

    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

	uint64_t end_page = (entry->base + entry->length) / PAGE_SIZE;
	if (end_page > pmm_max_page) {
	    pmm_max_page = end_page;
	}

	uint64_t pages = entry->length / PAGE_SIZE;
	total_pages += pages;

	if (entry->type == LIMINE_MEMMAP_USABLE) {
	    usable_pages += pages;
	}

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_usable_memory += entry->length;
        }
    }

    pmm_bitmap_size = (total_pages + 7) / 8;

    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

        if (entry->type == LIMINE_MEMMAP_USABLE &&
            entry->length >= pmm_bitmap_size) {
            pmm_bitmap = (uint8_t*)entry->base;
            break;
        }
    }

    if (!pmm_bitmap) {
        console_print("PMM ERROR: No space for bitmap.\n");
        return;
    }

    // Mark all pages used by default
    for (uint64_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0xFF;
    }

    // Mark usable pages free
    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start_page = entry->base / PAGE_SIZE;
        uint64_t page_count = entry->length / PAGE_SIZE;

        for (uint64_t p = 0; p < pmm_max_page; p++) {
            bitmap_clear(start_page + p);
        }
    }

    // Reserve bitmap itself
    uint64_t bitmap_start_page = (uint64_t)pmm_bitmap / PAGE_SIZE;
    uint64_t bitmap_page_count = (pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_page_count; i++) {
        bitmap_set(bitmap_start_page + i);
    }

    console_print("PMM bitmap initialized\n");
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

    console_print("Total pages: ");
    print_dec(total_pages);
    console_print("\n");

    console_print("Usable pages: ");
    print_dec(usable_pages);
    console_print("\n");
}

uint64_t memory_get_total_pages(void) {
    return total_pages;
}

uint64_t memory_get_usable_pages(void) {
    return usable_pages;
}

void* pmm_alloc_page(void) {
    if (!pmm_bitmap) {
        return 0;
    }

    for (uint64_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return (void*)(i * PAGE_SIZE);
        }
    }

    return 0; // out of memory
}

void pmm_free_page(void* addr) {
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    bitmap_clear(page);
}

uint64_t memory_get_free_pages(void) {
    uint64_t free = 0;

    for (uint64_t i = 0; i < pmm_max_page; i++) {
        if (!bitmap_test(i)) {
            free++;
        }
    }

    return free;
}
