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
static uint64_t kernel_cr3 = 0;

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

static int map_page_in_root(uint64_t* root, uint64_t virt, uint64_t phys, uint64_t flags) {
    if (!root) return 0;

    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    uint64_t table_flags = PTE_PRESENT | PTE_WRITABLE;
    if (flags & PTE_USER) table_flags |= PTE_USER;
    uint64_t* pdpt = (uint64_t*)(root[pml4_idx] & ~0xFFFULL);
    if ((root[pml4_idx] & PTE_PRESENT) == 0) {
        pdpt = alloc_page_table();
        if (!pdpt) return 0;
        root[pml4_idx] = (uint64_t)pdpt | table_flags;
    } else if (flags & PTE_USER) {
        root[pml4_idx] |= PTE_USER;
    }

    uint64_t* pd = (uint64_t*)(pdpt[pdpt_idx] & ~0xFFFULL);
    if ((pdpt[pdpt_idx] & PTE_PRESENT) == 0) {
        pd = alloc_page_table();
        if (!pd) return 0;
        pdpt[pdpt_idx] = (uint64_t)pd | table_flags;
    } else if (flags & PTE_USER) {
        pdpt[pdpt_idx] |= PTE_USER;
    }

    uint64_t* pt = (uint64_t*)(pd[pd_idx] & ~0xFFFULL);
    if ((pd[pd_idx] & PTE_PRESENT) == 0) {
        pt = alloc_page_table();
        if (!pt) return 0;
        pd[pd_idx] = (uint64_t)pt | table_flags;
    } else if (flags & PTE_USER) {
        pd[pd_idx] |= PTE_USER;
    }

    pt[pt_idx] = (phys & ~0xFFFULL) | (flags & (0xFFFULL | PTE_NX)) | PTE_PRESENT;
    invlpg(virt);
    return 1;
}

int paging_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    return map_page_in_root(pml4, virt, phys, flags);
}

int paging_map_page_in(uint64_t cr3, uint64_t virt, uint64_t phys, uint64_t flags) {
    return map_page_in_root((uint64_t*)(cr3 & ~0xFFFULL), virt, phys, flags);
}

static uint64_t* clone_table(const uint64_t* source) {
    uint64_t* copy = alloc_page_table();
    if (!copy) return 0;
    for (int i = 0; i < 512; i++) copy[i] = source[i];
    return copy;
}

uint64_t paging_create_user_address_space(void) {
    if (!pml4) return 0;
    uint64_t* root = clone_table(pml4);
    if (!root) return 0;
    if (pml4[0] & PTE_PRESENT) {
        uint64_t* old_pdpt = (uint64_t*)(pml4[0] & ~0xFFFULL);
        uint64_t* new_pdpt = clone_table(old_pdpt);
        if (!new_pdpt) { pmm_free_page(root); return 0; }
        root[0] = (uint64_t)new_pdpt | (pml4[0] & 0xFFFULL);
        if (old_pdpt[0] & PTE_PRESENT) {
            uint64_t* old_pd = (uint64_t*)(old_pdpt[0] & ~0xFFFULL);
            uint64_t* new_pd = clone_table(old_pd);
            if (!new_pd) { pmm_free_page(new_pdpt); pmm_free_page(root); return 0; }
            new_pdpt[0] = (uint64_t)new_pd | (old_pdpt[0] & 0xFFFULL);
            for (int i = 0; i < 512; i++) {
                if ((old_pd[i] & PTE_PRESENT) && !(old_pd[i] & PTE_HUGE)) {
                    uint64_t* new_pt = clone_table((uint64_t*)(old_pd[i] & ~0xFFFULL));
                    if (!new_pt) return 0;
                    new_pd[i] = (uint64_t)new_pt | (old_pd[i] & 0xFFFULL);
                }
            }
        }
    }
    return (uint64_t)root;
}

void paging_destroy_user_address_space(uint64_t cr3) {
    uint64_t root_phys = cr3 & ~0xFFFULL;
    if (!root_phys || root_phys == kernel_cr3) return;
    uint64_t* root = (uint64_t*)root_phys;
    /* Slot zero is eagerly cloned by paging_create_user_address_space(),
       including its supervisor PT pages; free those table copies but only
       free leaf frames carrying PTE_USER. */
    if (root[0] & PTE_PRESENT) {
        uint64_t* pdpt=(uint64_t*)(root[0] & ~0xFFFULL);
        if ((pdpt[0] & PTE_PRESENT) && !(pdpt[0] & PTE_HUGE)) {
            uint64_t* pd=(uint64_t*)(pdpt[0] & ~0xFFFULL);
            for (int c=0;c<512;c++) if ((pd[c]&PTE_PRESENT) && !(pd[c]&PTE_HUGE)) {
                uint64_t* pt=(uint64_t*)(pd[c]&~0xFFFULL);
                for(int d=0;d<512;d++)
                    if((pt[d]&(PTE_PRESENT|PTE_USER))==(PTE_PRESENT|PTE_USER))
                        pmm_free_page((void*)(pt[d]&0x000FFFFFFFFFF000ULL));
                pmm_free_page(pt);
            }
            pmm_free_page(pd);
        }
        pmm_free_page(pdpt);
    }
    /* Other user-marked PML4 branches were allocated on demand (not cloned),
       notably the high userspace stack branch used by ELF processes. */
    for (int a=1; a<512; a++) {
        if ((root[a] & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER) ||
            (root[a] & PTE_HUGE)) continue;
        uint64_t* pdpt=(uint64_t*)(root[a] & ~0xFFFULL);
        for (int b=0; b<512; b++) {
            if ((pdpt[b] & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER) ||
                (pdpt[b] & PTE_HUGE)) continue;
            uint64_t* pd=(uint64_t*)(pdpt[b] & ~0xFFFULL);
            for (int c=0; c<512; c++) {
                if ((pd[c] & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER) ||
                    (pd[c] & PTE_HUGE)) continue;
                uint64_t* pt=(uint64_t*)(pd[c] & ~0xFFFULL);
                for (int d=0; d<512; d++)
                    if ((pt[d] & (PTE_PRESENT|PTE_USER)) == (PTE_PRESENT|PTE_USER))
                        pmm_free_page((void*)(pt[d] & 0x000FFFFFFFFFF000ULL));
                pmm_free_page(pt);
            }
            pmm_free_page(pd);
        }
        pmm_free_page(pdpt);
    }
    pmm_free_page(root);
}

void paging_activate(uint64_t cr3) {
    if (cr3) __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3 & ~0xFFFULL) : "memory");
}

uint64_t paging_get_kernel_cr3(void) { return kernel_cr3; }

uint64_t paging_translate_in(uint64_t cr3, uint64_t virt, uint64_t* flags) {
    if (!cr3) return 0;
    uint64_t e4 = ((uint64_t*)(cr3 & ~0xFFFULL))[(virt >> 39) & 0x1FF];
    if (!(e4 & PTE_PRESENT)) return 0;
    uint64_t e3 = ((uint64_t*)(e4 & ~0xFFFULL))[(virt >> 30) & 0x1FF];
    if (!(e3 & PTE_PRESENT) || (e3 & PTE_HUGE)) return 0;
    uint64_t e2 = ((uint64_t*)(e3 & ~0xFFFULL))[(virt >> 21) & 0x1FF];
    if (!(e2 & PTE_PRESENT) || (e2 & PTE_HUGE)) return 0;
    uint64_t e1 = ((uint64_t*)(e2 & ~0xFFFULL))[(virt >> 12) & 0x1FF];
    if (!(e1 & PTE_PRESENT)) return 0;
    if (flags) *flags = e1 & (0xFFFULL | PTE_NX);
    return (e1 & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFFULL);
}

int paging_user_range_valid(uint64_t cr3, uint64_t addr, uint64_t size, int write_access) {
    if (!cr3 || size == 0 || addr >= 0x0000800000000000ULL || size > 0x0000800000000000ULL - addr) return 0;
    uint64_t last = (addr + size - 1) & ~0xFFFULL;
    for (uint64_t v = addr & ~0xFFFULL;; v += PAGE_SIZE) {
        uint64_t* l4 = (uint64_t*)(cr3 & ~0xFFFULL);
        uint64_t e4 = l4[(v >> 39) & 0x1FF];
        if ((e4 & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER)) return 0;
        uint64_t e3 = ((uint64_t*)(e4 & ~0xFFFULL))[(v >> 30) & 0x1FF];
        if ((e3 & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER) || (e3 & PTE_HUGE)) return 0;
        uint64_t e2 = ((uint64_t*)(e3 & ~0xFFFULL))[(v >> 21) & 0x1FF];
        if ((e2 & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER) || (e2 & PTE_HUGE)) return 0;
        uint64_t e1 = ((uint64_t*)(e2 & ~0xFFFULL))[(v >> 12) & 0x1FF];
        if ((e1 & (PTE_PRESENT|PTE_USER)) != (PTE_PRESENT|PTE_USER)) return 0;
        if (write_access && !(e1 & PTE_WRITABLE)) return 0;
        if (v == last) break;
    }
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

    // Identity-map only physical memory that can actually exist according to
    // the boot memory map. Mapping a hard-coded 4 GiB with 4 KiB leaves wasted
    // thousands of page-table frames on small machines.
    uint64_t low_limit = memory_get_max_physical();

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
    // The linked kernel occupies only the beginning of its 2 GiB high-half
    // window. Limine/module/framebuffer mappings are preserved explicitly.
    uint64_t kernel_map_size = 64ULL * 1024 * 1024;
    for (uint64_t off = 0; off < kernel_map_size; off += PAGE_SIZE) {
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
    kernel_cr3 = (uint64_t)pml4;

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
