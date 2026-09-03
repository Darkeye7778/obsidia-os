#include "elf.h"
#include "paging.h"
#include "memory/memory.h"
#include "memory/heap.h"

#define PAGE_SIZE 4096ULL
#define USER_TOP 0x0000800000000000ULL
#define USER_STACK_BASE 0x00007FFFFF800000ULL
#define USER_STACK_PAGES 8
#define PT_LOAD 1
#define PF_W 2
#define PF_X 1
#define PTE_P 1ULL
#define PTE_W 2ULL
#define PTE_U 4ULL
#define PTE_NX (1ULL << 63)

typedef struct __attribute__((packed)) {
    uint8_t ident[16]; uint16_t type, machine; uint32_t version;
    uint64_t entry, phoff, shoff; uint32_t flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} elf64_header_t;
typedef struct __attribute__((packed)) {
    uint32_t type, flags; uint64_t offset, vaddr, paddr;
    uint64_t filesz, memsz, align;
} elf64_phdr_t;

static int add_ok(uint64_t a, uint64_t b, uint64_t limit) {
    return a <= limit && b <= limit - a;
}

static int map_zero_range(uint64_t cr3, uint64_t start, uint64_t end, uint64_t flags) {
    uint64_t first = start & ~(PAGE_SIZE - 1), last = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    for (uint64_t va = first; va < last; va += PAGE_SIZE) {
        uint64_t old_flags = 0;
        uint64_t phys = paging_translate_in(cr3, va, &old_flags);
        if (!(phys && (old_flags & PTE_U))) {
            void* page = pmm_alloc_page();
            if (!page || !paging_map_page_in(cr3, va, (uint64_t)page, flags)) return 0;
            for (uint64_t i=0; i<PAGE_SIZE; i++) ((uint8_t*)page)[i]=0;
        } else {
            /* Overlapping PT_LOAD pages retain the least restrictive flags. */
            uint64_t merged = (old_flags | flags) & ~PTE_NX;
            if ((old_flags & PTE_NX) && (flags & PTE_NX)) merged |= PTE_NX;
            if (!paging_map_page_in(cr3, va, phys & ~(PAGE_SIZE-1), merged)) return 0;
        }
    }
    return 1;
}

int elf_load_process(vfs_node_t* node, uint64_t* cr3_out,
                     uint64_t* entry_out, uint64_t* stack_top_out) {
    if (!node || node->type != VFS_FILE || node->size < sizeof(elf64_header_t)) return 0;
    uint8_t* image = kmalloc(node->size);
    if (!image) return 0;
    if (vfs_read(node, 0, image, node->size) != (int64_t)node->size) { kfree(image); return 0; }
    elf64_header_t* h = (elf64_header_t*)image;
    if (h->ident[0]!=0x7f || h->ident[1]!='E' || h->ident[2]!='L' || h->ident[3]!='F' ||
        h->ident[4]!=2 || h->ident[5]!=1 || h->machine!=62 || h->version!=1 ||
        h->phentsize!=sizeof(elf64_phdr_t) || !h->phnum ||
        !add_ok(h->phoff, (uint64_t)h->phnum*h->phentsize, node->size)) { kfree(image); return 0; }
    uint64_t cr3 = paging_create_user_address_space();
    if (!cr3) { kfree(image); return 0; }
    elf64_phdr_t* ph = (elf64_phdr_t*)(image + h->phoff);
    int loaded = 0;
    for (uint16_t i=0; i<h->phnum; i++) {
        if (ph[i].type != PT_LOAD) continue;
        if (ph[i].memsz < ph[i].filesz || !add_ok(ph[i].offset,ph[i].filesz,node->size) ||
            ph[i].vaddr < 0x10000 || !add_ok(ph[i].vaddr,ph[i].memsz,USER_TOP) ||
            ph[i].vaddr + ph[i].memsz > USER_STACK_BASE) goto fail;
        uint64_t pf = PTE_P|PTE_U | ((ph[i].flags&PF_W)?PTE_W:0) | ((ph[i].flags&PF_X)?0:PTE_NX);
        if (!map_zero_range(cr3,ph[i].vaddr,ph[i].vaddr+ph[i].memsz,pf)) goto fail;
        for (uint64_t j=0; j<ph[i].filesz; j++) {
            uint64_t dst = paging_translate_in(cr3,ph[i].vaddr+j,0);
            if (!dst) goto fail;
            *(uint8_t*)dst = image[ph[i].offset+j];
        }
        loaded=1;
    }
    if (!loaded || h->entry < 0x10000 || h->entry >= USER_STACK_BASE ||
        !paging_translate_in(cr3,h->entry,0)) goto fail;
    if (!map_zero_range(cr3,USER_STACK_BASE,USER_STACK_BASE+USER_STACK_PAGES*PAGE_SIZE,PTE_P|PTE_U|PTE_W|PTE_NX)) goto fail;
    *cr3_out=cr3; *entry_out=h->entry; *stack_top_out=USER_STACK_BASE+USER_STACK_PAGES*PAGE_SIZE;
    kfree(image); return 1;
fail:
    paging_destroy_user_address_space(cr3); kfree(image); return 0;
}
