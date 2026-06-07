#include "heap.h"
#include "memory.h"
#include "../console/console.h"

#define PAGE_SIZE 4096
#define HEAP_INITIAL_PAGES 16
#define HEAP_ALIGN 16
#define BLOCK_MAGIC 0xA11C0C0AULL

typedef struct block_header {
    uint64_t magic;
    uint64_t size;              // user payload size (not including header)
    struct block_header* next;  // next in free list (0 if allocated or last)
} block_t;

static block_t* free_list = 0;
static uint8_t* heap_arena_end = 0;

static uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static block_t* block_from_ptr(void* ptr) {
    return (block_t*)((uint8_t*)ptr - sizeof(block_t));
}

static void* ptr_from_block(block_t* blk) {
    return (void*)((uint8_t*)blk + sizeof(block_t));
}

static void insert_free_block(block_t* blk) {
    // Keep list sorted by address for easy coalescing
    if (!free_list || (uint64_t)blk < (uint64_t)free_list) {
        blk->next = free_list;
        free_list = blk;
        return;
    }

    block_t* prev = free_list;
    block_t* cur = free_list->next;
    while (cur && (uint64_t)cur < (uint64_t)blk) {
        prev = cur;
        cur = cur->next;
    }
    blk->next = cur;
    prev->next = blk;
}

static void coalesce(block_t* blk) {
    // Coalesce with following block if adjacent
    if (blk->next && (uint8_t*)blk + sizeof(block_t) + blk->size == (uint8_t*)blk->next) {
        blk->size += sizeof(block_t) + blk->next->size;
        blk->next = blk->next->next;
    }
}

void heap_init(void) {
    free_list = 0;
    heap_arena_end = 0;

    // Allocate initial arena pages and create one large free block
    block_t* first = 0;
    for (uint64_t i = 0; i < HEAP_INITIAL_PAGES; i++) {
        void* page = pmm_alloc_page();
        if (!page) {
            console_print("HEAP: failed to alloc initial pages\n");
            return;
        }

        if (i == 0) {
            first = (block_t*)page;
            heap_arena_end = (uint8_t*)page + PAGE_SIZE;
        } else {
            heap_arena_end = (uint8_t*)page + PAGE_SIZE;
        }
    }

    if (!first) return;

    // One big free block covering the initial arena
    uint64_t arena_size = (uint64_t)heap_arena_end - (uint64_t)first;
    // Make room for header
    uint64_t payload = arena_size - sizeof(block_t);
    payload = (payload / HEAP_ALIGN) * HEAP_ALIGN;

    first->magic = BLOCK_MAGIC;
    first->size = payload;
    first->next = 0;

    free_list = first;
}

static int expand_heap(uint64_t needed) {
    // Allocate more pages from PMM and append a new free block at end of arena
    uint64_t pages_needed = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed < 1) pages_needed = 1;

    block_t* new_block = 0;
    for (uint64_t i = 0; i < pages_needed; i++) {
        void* p = pmm_alloc_page();
        if (!p) return 0;

        if (i == 0) {
            new_block = (block_t*)p;
        }
        heap_arena_end = (uint8_t*)p + PAGE_SIZE;
    }

    if (!new_block) return 0;

    uint64_t block_total = (uint64_t)heap_arena_end - (uint64_t)new_block;
    uint64_t payload = block_total - sizeof(block_t);
    payload = (payload / HEAP_ALIGN) * HEAP_ALIGN;

    new_block->magic = BLOCK_MAGIC;
    new_block->size = payload;
    new_block->next = 0;

    insert_free_block(new_block);
    coalesce(new_block); // may merge with previous if we were lucky on addr, but usually new high addr
    return 1;
}

void* kmalloc(uint64_t size) {
    if (size == 0) return 0;

    if (!free_list) {
        heap_init();
        if (!free_list) return 0;
    }

    size = align_up(size, HEAP_ALIGN);

    // First fit
    block_t* prev = 0;
    block_t* cur = free_list;

    while (cur) {
        if (cur->magic != BLOCK_MAGIC) {
            // corruption guard
            return 0;
        }
        if (cur->size >= size) {
            // Found a block big enough
            uint64_t remaining = cur->size - size;

            if (remaining >= sizeof(block_t) + HEAP_ALIGN) {
                // Split
                block_t* new_free = (block_t*)((uint8_t*)cur + sizeof(block_t) + size);
                new_free->magic = BLOCK_MAGIC;
                new_free->size = remaining - sizeof(block_t);
                new_free->next = cur->next;

                cur->size = size;
                cur->next = 0; // will be allocated

                if (prev) prev->next = new_free;
                else free_list = new_free;
            } else {
                // Take whole block (no split worth it)
                if (prev) prev->next = cur->next;
                else free_list = cur->next;
                cur->next = 0;
            }

            cur->magic = BLOCK_MAGIC; // keep for debug
            return ptr_from_block(cur);
        }

        prev = cur;
        cur = cur->next;
    }

    // No suitable block: expand
    if (!expand_heap(size + sizeof(block_t))) {
        return 0;
    }

    // Retry once after expand (simple)
    return kmalloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_t* blk = block_from_ptr(ptr);
    if (blk->magic != BLOCK_MAGIC) {
        // double free or bad ptr guard
        return;
    }

    // Mark as free by putting in list (next will be set by insert)
    blk->next = 0; // temp
    insert_free_block(blk);

    // Try coalescing with neighbors (the insert sorted helps)
    // Coalesce this with next
    coalesce(blk);

    // Also try to coalesce the previous one that might now be adjacent to us
    // Walk to find potential prev
    block_t* p = free_list;
    while (p && p->next != blk) p = p->next;
    if (p) {
        coalesce(p);
    }
}
