#include "paging.h"
#include "memory/memory.h"
#include "memory/heap.h"
#include "console/console.h"
#include "drivers/framebuffer.h"
#include "gdt.h"
#include "limine.h"
#include <stdint.h>

void serial_write(const char *str);

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

// Translate a virtual address to physical using the *current* CR3 (Limine's tables).
// This is used during early paging setup to correctly map the boot stack etc.
// Assumes the current tables allow accessing table pages via their physical addresses
// (standard for Limine with identity mapping on low RAM).
static uint64_t current_virt_to_phys(uint64_t virt) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    uint64_t pml4_phys = cr3 & ~0xFFFULL;

    // Walk 4 levels. We cast physical addresses of tables to pointers.
    // This works because Limine sets up identity mapping for the memory containing the tables (low RAM).
    uint64_t pml4e = ((uint64_t *)pml4_phys)[(virt >> 39) & 0x1FF];
    if (!(pml4e & PTE_PRESENT)) return 0;

    uint64_t pdpt_phys = pml4e & ~0xFFFULL;
    uint64_t pdpte = ((uint64_t *)pdpt_phys)[(virt >> 30) & 0x1FF];
    if (!(pdpte & PTE_PRESENT)) return 0;
    if (pdpte & PTE_HUGE) {
        return (pdpte & ~0x3FFFFF) | (virt & 0x3FFFFF);
    }

    uint64_t pd_phys = pdpte & ~0xFFFULL;
    uint64_t pde = ((uint64_t *)pd_phys)[(virt >> 21) & 0x1FF];
    if (!(pde & PTE_PRESENT)) return 0;
    if (pde & PTE_HUGE) {
        return (pde & ~0x1FFFFF) | (virt & 0x1FFFFF);
    }

    uint64_t pt_phys = pde & ~0xFFFULL;
    uint64_t pte = ((uint64_t *)pt_phys)[(virt >> 12) & 0x1FF];
    if (!(pte & PTE_PRESENT)) return 0;

    return (pte & ~0xFFFULL) | (virt & 0xFFFULL);
}

static inline void invlpg(uint64_t addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

void paging_invlpg(uint64_t addr) {
    invlpg(addr);
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

// Re-map [vaddr, vaddr+size) using the exact same physical frames that back
// them under the *current* CR3 (Limine tables). This makes Limine-provided
// pointers (response structs, module arrays, file descriptors, and the actual
// initrd blob bytes) continue to be valid after we switch to our CR3.
int paging_preserve_range(uint64_t vaddr, uint64_t size) {
    if (size == 0 || !pml4) return 0;
    uint64_t start = vaddr & ~0xFFFULL;
    uint64_t end = (vaddr + size + 0xFFFULL) & ~0xFFFULL;
    for (uint64_t v = start; v < end; v += PAGE_SIZE) {
        uint64_t p = current_virt_to_phys(v);
        if (p == 0) {
            // Fallback: assume identity for this page (common for Limine low RAM placements)
            p = v;
        }
        paging_map_page(v, p, PTE_PRESENT | PTE_WRITABLE);
    }
    return 1;
}

void paging_preserve_limine_modules(void) {
    extern volatile struct limine_module_request module_request;

    if (!module_request.response) {
        return;
    }

    // Preserve the response struct itself (at least its page)
    paging_preserve_range((uint64_t)module_request.response, 4096);

    struct limine_module_response *resp = module_request.response;
    uint64_t cnt = resp->module_count;
    if (cnt == 0 || !resp->modules) {
        return;
    }

    // Preserve the array of module pointers
    paging_preserve_range((uint64_t)resp->modules, (cnt + 1) * sizeof(struct limine_file *));

    for (uint64_t i = 0; i < cnt; i++) {
        struct limine_file *f = resp->modules[i];
        if (!f) continue;
        // Preserve the limine_file descriptor struct
        paging_preserve_range((uint64_t)f, 4096);
        uint64_t addr = (uint64_t)f->address;
        uint64_t sz = f->size;
        if (addr && sz) {
            // Preserve the entire module payload bytes (the initrd OAR blob)
            uint64_t end = addr + sz;
            uint64_t vstart = addr & ~0xFFFULL;
            uint64_t vend = (end + 0xFFFULL) & ~0xFFFULL;
            for (uint64_t v = vstart; v < vend; v += PAGE_SIZE) {
                uint64_t p = current_virt_to_phys(v);
                if (p == 0) p = v;
                paging_map_page(v, p, PTE_PRESENT | PTE_WRITABLE);
            }
        }
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

    // 4 GiB low identity + high kernel alias to cover Limine responses, modules, FB MMIO etc.
    // (plus explicit preserve for non-identity pointers from bootloader)
    uint64_t low_limit = 0x100000000ULL;

    uint64_t* pd_low = alloc_page_table();
    if (!pd_low) return;
    pdpt_low[0] = (uint64_t)pd_low | PTE_PRESENT | PTE_WRITABLE;

    map_range_identity(0, low_limit, 0);

    // Compute the actual physical base address of the kernel using the current (Limine) page tables.
    // Limine loads the kernel at a low physical address (not 0), so high-half mappings must use the correct base.
    uint64_t kernel_code_virt = (uint64_t)&paging_init;
    uint64_t kernel_code_phys = current_virt_to_phys(kernel_code_virt);
    uint64_t kernel_phys_base = kernel_code_phys - (kernel_code_virt - KERNEL_VMA);

    // High half kernel alias: map KERNEL_VMA + off -> kernel_phys_base + off
    for (uint64_t off = 0; off < low_limit; off += PAGE_SIZE) {
        uint64_t v = KERNEL_VMA + off;
        uint64_t p = kernel_phys_base + off;
        paging_map_page(v, p, PTE_WRITABLE | PTE_GLOBAL);
    }

    // Preserve the exact virtual address Limine gave for the framebuffer (in the response)
    // so that the captured 'fb' pointer and g_fb_phys_base in the driver remain valid
    // after CR3. Use current_virt_to_phys to get the real underlying phys, then map
    // access_virt -> real_phys with MMIO flags.
    uint64_t fb_access_virt = fb_get_phys_base();
    if (fb_access_virt) {
        uint64_t fb_map_size = 16ULL * 1024 * 1024;
        paging_preserve_range(fb_access_virt, fb_map_size);
        for (uint64_t off = 0; off < fb_map_size; off += PAGE_SIZE) {
            uint64_t v = fb_access_virt + off;
            uint64_t p = current_virt_to_phys(v);
            if (p == 0) p = v;
            paging_map_page(v, p, PTE_WRITABLE | PTE_PCD | PTE_PWT);
        }
    }

    // Preserve Limine module_request data (response + module descriptors + payload bytes)
    // while we are still on the old CR3 so current_virt_to_phys can resolve them.
    paging_preserve_limine_modules();

    // Set CR4 (PAE + PGE) *before* CR3 for a clean long-mode paging transition
    uint64_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= (1 << 5) /* PAE */ | (1 << 7) /* PGE */;
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4) : "memory");

    // Properly translate the current stack virtual address to its physical address
    // using the *current* (Limine) page tables, then map it in our new tables.
    uint64_t rsp;
    __asm__ volatile ("mov %%rsp, %0" : "=r"(rsp));
    uint64_t stack_page_v = rsp & ~0xFFFULL;
    uint64_t stack_phys = current_virt_to_phys(stack_page_v);
    if (stack_phys == 0) {
        stack_phys = stack_page_v;
    }
    paging_map_page(stack_page_v, stack_phys, PTE_WRITABLE);

    __asm__ volatile (
        "mov %0, %%cr3\n"
        "mov %%cr3, %%rax\n"
        "mov %%rax, %%cr3\n"
        : : "r"((uint64_t)pml4) : "rax", "memory"
    );

    // Reload GDT/segments under the new page tables (important after CR3 in higher-half)
    gdt_reload();

    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1ULL << 31) /* PG */ | (1ULL << 16) /* WP */;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0) : "memory");

    // Force RIP reload under the new page tables *after* PG is enabled.
    __asm__ volatile (
        "mov $1f, %%rax\n\t"
        "push %%rax\n\t"
        "ret\n"
        "1:\n\t"
        ::: "rax", "memory"
    );

    serial_write("Paging enabled\n");
}

uint64_t paging_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
