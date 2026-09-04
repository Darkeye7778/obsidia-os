#include "memory.h"
#include "../console/console.h"
#include <stdbool.h>  // for bool in pmm_alloc_pages (freestanding safe)

#define PAGE_SIZE 4096

static uint64_t total_pages = 0;
static uint64_t usable_pages = 0;

static struct limine_memmap_response* g_memmap = 0;
static uint64_t total_usable_memory = 0;

static uint8_t* pmm_bitmap = 0;
static uint64_t pmm_bitmap_size = 0;

static uint64_t pmm_max_page = 0;
static uint64_t pmm_usable_limit = 0;
static uint64_t pmm_next_hint = 0;
static uint64_t pmm_free_count = 0;
typedef struct { uint64_t first, end; } usable_range_t;
static usable_range_t usable_ranges[128];
static uint32_t usable_range_count;

static uint64_t irq_save(void) { uint64_t f; __asm__ volatile("pushfq; pop %0; cli":"=r"(f)::"memory"); return f; }
static void irq_restore(uint64_t f) { if(f&(1ULL<<9)) __asm__ volatile("sti":::"memory"); }

static int page_is_usable(uint64_t page) {
    uint64_t addr=page*PAGE_SIZE;
    for(uint32_t i=0;i<usable_range_count;i++) {
        if(addr>=usable_ranges[i].first && addr<usable_ranges[i].end) return 1;
    }
    return 0;
}

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
    usable_range_count = 0;

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
	    if (usable_range_count < 128) {
	        usable_ranges[usable_range_count].first = entry->base;
	        usable_ranges[usable_range_count].end = entry->base + entry->length;
	        usable_range_count++;
	    }
	    usable_pages += pages;
	    uint64_t usable_end = entry->base + entry->length;
	    if (usable_end > pmm_usable_limit) pmm_usable_limit = usable_end;
	}

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            total_usable_memory += entry->length;
        }
    }

    pmm_bitmap_size = (pmm_max_page + 7) / 8;

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

    uint64_t bitmap_start_page = (uint64_t)pmm_bitmap / PAGE_SIZE;
    uint64_t bitmap_page_count = (pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Mark usable pages free, except the pages that physically contain the
    // bitmap itself. Skipping them here avoids self-referential bitmap writes
    // changing their own allocation state while the map is initialized.
    for (uint64_t i = 0; i < g_memmap->entry_count; i++) {
        struct limine_memmap_entry* entry = g_memmap->entries[i];

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uint64_t start_page = entry->base / PAGE_SIZE;
        uint64_t page_count = entry->length / PAGE_SIZE;

        for (uint64_t p = 0; p < page_count; p++) {
            uint64_t page = start_page + p;
            if (page >= bitmap_start_page && page < bitmap_start_page + bitmap_page_count) continue;
            bitmap_clear(page);
            pmm_free_count++;
        }
    }

    // The bitmap pages remained marked used from the initial 0xFF fill.
    for (uint64_t i = 0; i < bitmap_page_count; i++) {
        if (!bitmap_test(bitmap_start_page + i)) {
            bitmap_set(bitmap_start_page + i);
            pmm_free_count--;
        }
    }

    pmm_next_hint = 0;
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
    uint64_t irq=irq_save();
    if (!pmm_bitmap || pmm_free_count == 0) {
        irq_restore(irq);
        return 0;
    }

    uint64_t start = pmm_next_hint;
    for (uint64_t i = 0; i < pmm_max_page; i++) {
        uint64_t p = (start + i) % pmm_max_page;
        if (!bitmap_test(p)) {
            bitmap_set(p);
            pmm_free_count--;
            pmm_next_hint = (p + 1) % pmm_max_page;
            irq_restore(irq);
            return (void*)(p * PAGE_SIZE);
        }
    }

    irq_restore(irq);
    return 0;
}

void pmm_free_page(void* addr) {
    if (!addr || ((uint64_t)addr & (PAGE_SIZE-1))) return;
    uint64_t irq=irq_save();
    uint64_t page = (uint64_t)addr / PAGE_SIZE;
    if (page >= pmm_max_page || !page_is_usable(page)) { irq_restore(irq); return; }
    if (bitmap_test(page)) {
        bitmap_clear(page);
        pmm_free_count++;
    }
    irq_restore(irq);
}

void* pmm_alloc_pages(uint64_t count) {
    uint64_t irq=irq_save();
    if (!pmm_bitmap || count == 0 || pmm_free_count < count || count>pmm_max_page) { irq_restore(irq); return 0; }
    if (count == 1) { irq_restore(irq); return pmm_alloc_page(); }
    uint64_t run=0, base=0;
    for(uint64_t scanned=0;scanned<pmm_max_page;scanned++) {
        uint64_t p=(pmm_next_hint+scanned)%pmm_max_page;
        if(!bitmap_test(p)) { if(!run) base=p; run++; }
        else run=0;
        if(run==count) {
            for(uint64_t j=0;j<count;j++) bitmap_set(base+j);
            pmm_free_count-=count; pmm_next_hint=(base+count)%pmm_max_page;
            irq_restore(irq); return (void*)(base*PAGE_SIZE);
        }
        if(p==pmm_max_page-1) run=0; /* never wrap a physical run */
    }
    irq_restore(irq); return 0;
}

void pmm_free_pages(void* addr, uint64_t count) {
    if (!addr || count == 0 || ((uint64_t)addr&(PAGE_SIZE-1))) return;
    uint64_t irq=irq_save();
    uint64_t start = (uint64_t)addr / PAGE_SIZE;
    for (uint64_t j = 0; j < count; j++) {
        uint64_t p = start + j;
        if (p < pmm_max_page && page_is_usable(p) && bitmap_test(p)) {
            bitmap_clear(p);
            pmm_free_count++;
        }
    }
    irq_restore(irq);
}

uint64_t memory_get_free_pages(void) {
    return pmm_free_count;
}

uint64_t memory_get_max_physical(void) {
    return pmm_usable_limit;
}
