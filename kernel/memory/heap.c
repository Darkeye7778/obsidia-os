#include "heap.h"
#include "memory.h"

#define PAGE_SIZE 4096
#define HEAP_INITIAL_PAGES 16

static uint8_t* heap_start = 0;
static uint8_t* heap_current = 0;
static uint8_t* heap_end = 0;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void heap_init(void) {
    heap_start = 0;
    heap_current = 0;
    heap_end = 0;

    for (uint64_t i = 0; i < HEAP_INITIAL_PAGES; i++) {
        void* page = pmm_alloc_page();

        if (!page) {
            return;
        }

        if (i == 0) {
            heap_start = (uint8_t*)page;
            heap_current = heap_start;
        }

        heap_end = (uint8_t*)page + PAGE_SIZE;
    }
}

void* kmalloc(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    if (!heap_current) {
        heap_init();
    }

    uint64_t aligned_current = align_up((uint64_t)heap_current, 16);
    uint64_t new_current = aligned_current + size;

    if (new_current > (uint64_t)heap_end) {
        void* page = pmm_alloc_page();

        if (!page) {
            return 0;
        }

        heap_current = (uint8_t*)page;
        heap_end = heap_current + PAGE_SIZE;

        aligned_current = align_up((uint64_t)heap_current, 16);
        new_current = aligned_current + size;
    }

    heap_current = (uint8_t*)new_current;

    return (void*)aligned_current;
}

void kfree(void* ptr) {
    (void)ptr;
    // No-op for now. This is a bump allocator.
}
