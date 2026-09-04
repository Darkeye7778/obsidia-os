#include "heap.h"
#include "memory.h"
#include "../console/console.h"

#define PAGE_SIZE 4096
#define HEAP_INITIAL_PAGES 16
#define HEAP_ALIGN 16
#define BLOCK_FREE_MAGIC  0xF4EEB10CF4EEB10CULL
#define BLOCK_ALLOC_MAGIC 0xA110CA7EA110CA7EULL

typedef struct block_header {
    uint64_t magic;
    uint64_t size;              // user payload size (not including header)
    struct block_header* next;  // next in free list (0 if allocated or last)
    uint64_t reserved;          // keeps returned storage 16-byte aligned
} block_t;

static block_t* free_list = 0;
static uint8_t* heap_arena_end = 0;

static uint64_t irq_save(void) { uint64_t f; __asm__ volatile("pushfq; pop %0; cli":"=r"(f)::"memory"); return f; }
static void irq_restore(uint64_t f) { if(f&(1ULL<<9)) __asm__ volatile("sti":::"memory"); }

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
    block_t* first = (block_t*)pmm_alloc_pages(HEAP_INITIAL_PAGES);
    if (!first) {
        console_print("HEAP: failed to allocate contiguous initial arena\n");
        return;
    }
    heap_arena_end = (uint8_t*)first + HEAP_INITIAL_PAGES * PAGE_SIZE;

    if (!first) return;

    // One big free block covering the initial arena
    uint64_t arena_size = (uint64_t)heap_arena_end - (uint64_t)first;
    // Make room for header
    uint64_t payload = arena_size - sizeof(block_t);
    payload = (payload / HEAP_ALIGN) * HEAP_ALIGN;

    first->magic = BLOCK_FREE_MAGIC;
    first->size = payload;
    first->next = 0;

    free_list = first;
}

static int expand_heap(uint64_t needed) {
    // Allocate more pages from PMM and append a new free block at end of arena
    uint64_t pages_needed = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages_needed < 1) pages_needed = 1;

    block_t* new_block = (block_t*)pmm_alloc_pages(pages_needed);
    if (!new_block) return 0;
    heap_arena_end = (uint8_t*)new_block + pages_needed * PAGE_SIZE;

    uint64_t block_total = (uint64_t)heap_arena_end - (uint64_t)new_block;
    uint64_t payload = block_total - sizeof(block_t);
    payload = (payload / HEAP_ALIGN) * HEAP_ALIGN;

    new_block->magic = BLOCK_FREE_MAGIC;
    new_block->size = payload;
    new_block->next = 0;

    insert_free_block(new_block);
    coalesce(new_block); // may merge with previous if we were lucky on addr, but usually new high addr
    return 1;
}

static void* kmalloc_locked(uint64_t size) {
    if (size == 0) return 0;

    if (!free_list) {
        heap_init();
        if (!free_list) return 0;
    }

    size = align_up(size, HEAP_ALIGN);

retry:
    // First fit
    block_t* prev = 0;
    block_t* cur = free_list;

    while (cur) {
        if (cur->magic != BLOCK_FREE_MAGIC) {
            // corruption guard
            return 0;
        }
        if (cur->size >= size) {
            // Found a block big enough
            uint64_t remaining = cur->size - size;

            if (remaining >= sizeof(block_t) + HEAP_ALIGN) {
                // Split
                block_t* new_free = (block_t*)((uint8_t*)cur + sizeof(block_t) + size);
                new_free->magic = BLOCK_FREE_MAGIC;
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

            cur->magic = BLOCK_ALLOC_MAGIC;
            return ptr_from_block(cur);
        }

        prev = cur;
        cur = cur->next;
    }

    // No suitable block: expand
    if (!expand_heap(size + sizeof(block_t))) {
        return 0;
    }

    goto retry;
}

void* kmalloc(uint64_t size) {
    uint64_t irq=irq_save();
    void* p=kmalloc_locked(size);
    irq_restore(irq);
    return p;
}

void kfree(void* ptr) {
    if (!ptr || ((uint64_t)ptr & (HEAP_ALIGN-1))) return;
    uint64_t irq=irq_save();

    block_t* blk = block_from_ptr(ptr);
    if (blk->magic != BLOCK_ALLOC_MAGIC) {
        irq_restore(irq);
        return; // invalid pointer or double free
    }

    // Mark as free by putting in list (next will be set by insert)
    blk->magic = BLOCK_FREE_MAGIC;
    blk->next = 0;
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
    irq_restore(irq);
}
