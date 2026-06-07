#include "paging.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include "console/console.h"
#include <stdint.h>

#define PAGE_SIZE 4096
#define KERNEL_VMA 0xffffffff80000000ULL

// Page table entry flags
#define PTE_PRESENT  (1ULL << 0)
#define PTE_WRITABLE (1ULL << 1)
#define PTE_USER     (1ULL << 2)
#define PTE_PWT      (1ULL << 3)
#define PTE_PCD      (1ULL << 4)
#define PTE_ACCESSED (1ULL << 5)
#define PTE_DIRTY    (1ULL << 6)
#define PTE_HUGE     (1ULL << 7)  // 2MiB / 1GiB
#define PTE_GLOBAL   (1ULL << 8)
#define PTE_NX       (1ULL << 63)

static uint64_t* pml4 = 0;

static uint64_t* alloc_page_table(void) {
    void* p = pmm_alloc_page();
    if (!p) return 0;
    uint8_t* b = (uint8_t*)p;
    for (int i=0; i<PAGE_SIZE; i++) b[i]=0;
    return (uint64_t*)p;
}

static inline void invlpg(uint64_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

int paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!pml4) return 0;

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = (uint64_t*)(pml4[pml4_idx] & ~0xFFFULL);
    if ((pml4[pml4_idx] & PTE_PRESENT) == 0) {
        pdpt = alloc_page_table();
        if (!pdpt) return 0;
        pml4[pml4_idx] = (uint64_t)pdpt | PTE_PRESENT | PTE_WRITABLE;
    }

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if ((pdpt[pdpt_idx] & PTE_PRESENT) == 0) {
        pd = alloc_page_table();
        if (!pd) return 0;
        pdpt[pdpt_idx] = (uint64_t)pd | PTE_PRESENT | PTE_WRITABLE;
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);
    if ((pd[pd_idx] & PTE_PRESENT) == 0) {
        pt = alloc_page_table();
        if (!pt) return 0;
        pd[pd_idx] = (uint64_t)pt | PTE_PRESENT | PTE_WRITABLE;
    }

    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & 0xFFF) | PTE_PRESENT;
    invlpg(virt);
    return 1;
}

static void map_range_identity(uint64_t start, uint64_t end, uint64_t extra_flags) {
    for (uint64_t p = start; p < end; p += PAGE_SIZE) {
        paging_map_page(p, p, PTE_WRITABLE | extra_flags);
    }
}

void paging_init(void) {
    // Allocate top level tables
    pml4 = alloc_page_table();
    if (!pml4) {
        console_print("PAGING: failed to alloc PML4\n");
        return;
    }

    uint64_t* pdpt_low = alloc_page_table();
    uint64_t* pdpt_high = alloc_page_table();
    if (!pdpt_low || !pdpt_high) {
        console_print("PAGING: failed to alloc PDPTs\n");
        return;
    }

    // Wire PML4[0] for low identity, PML4[511] for high half
    pml4[0]   = (uint64_t)pdpt_low  | PTE_PRESENT | PTE_WRITABLE;
    pml4[511] = (uint64_t)pdpt_high | PTE_PRESENT | PTE_WRITABLE;

    // For low identity: use a few PDs (cover ~ 4GB worst case but we do 1GB for base)
    // Simpler: allocate 4 PDs under low PDPT (indices 0-3) for 4*1GB = but we use 2MiB huge where easy, fall back to 4k.
    // For robustness we do 4k pages for first 256 MiB (enough for early kernel + modules + heap + FB on most VMs)
    uint64_t low_limit = 256ULL * 1024 * 1024; // 256 MiB identity

    // Create PD for low 0-1GB range (we only populate first ~256M)
    uint64_t* pd_low = alloc_page_table();
    if (!pd_low) return;
    pdpt_low[0] = (uint64_t)pd_low | PTE_PRESENT | PTE_WRITABLE;

    // Populate PTs or use the map helper (it will alloc PTs on demand)
    map_range_identity(0, low_limit, 0);

    // Map framebuffer (use the physical from early fb if we had it; for now map a large low range which will cover typical FB)
    // The fb address from Limine is usually in the low mapped area already.

    // High half kernel alias: map the kernel virtual range to the same low physical (so higher half symbols work)
    // Map 0xffffffff80000000 .. + low_limit  -> 0x0 .. low_limit
    // This lets us keep running after we enable the high mapping.
    for (uint64_t off = 0; off < low_limit; off += PAGE_SIZE) {
        uint64_t v = KERNEL_VMA + off;
        uint64_t p = off;
        paging_map_page(v, p, PTE_WRITABLE | PTE_GLOBAL);
    }

    // Load CR3
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint64_t)pml4) : "memory");

    // Enable PAE + PGE in CR4
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5) /* PAE */ | (1 << 7) /* PGE */;
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");

    // Enable PG + WP in CR0
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1ULL << 31) /* PG */ | (1ULL << 16) /* WP */;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    console_print("Paging enabled (higher-half + identity low 256MiB)\n");
}

uint64_t paging_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
