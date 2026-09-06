#include "image.h"
#include "obsx.h"
#include "pe.h"
#include "../elf.h"
#include "../paging.h"
#include "../memory/memory.h"
#include "../memory/heap.h"

#define PAGE_SIZE 4096ULL
#define USER_MIN 0x10000ULL
#define USER_TOP 0x0000800000000000ULL
#define USER_STACK_BASE 0x00007FFFFF800000ULL
#define USER_STACK_PAGES 8U
#define PTE_P 1ULL
#define PTE_W 2ULL
#define PTE_U 4ULL
#define PTE_NX (1ULL << 63)

extern void serial_write(const char* text);

static int add_ok(uint64_t a, uint64_t b, uint64_t limit) {
    return a <= limit && b <= limit - a;
}

static int map_segment_pages(uint64_t cr3, uint64_t start, uint64_t end,
                             uint32_t segment_flags) {
    uint64_t first = start & ~(PAGE_SIZE - 1);
    uint64_t last = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    uint64_t requested = PTE_P | PTE_U;
    if (segment_flags & EXEC_SEGMENT_WRITE) requested |= PTE_W;
    if (!(segment_flags & EXEC_SEGMENT_EXEC)) requested |= PTE_NX;
    for (uint64_t va = first; va < last; va += PAGE_SIZE) {
        uint64_t old_flags = 0;
        uint64_t phys = paging_translate_in(cr3, va, &old_flags);
        if (!phys || !(old_flags & PTE_U)) {
            void* page = pmm_alloc_page();
            if (!page) return 0;
            for (uint64_t i = 0; i < PAGE_SIZE; i++) ((uint8_t*)page)[i] = 0;
            if (!paging_map_page_in(cr3, va, (uint64_t)page, requested)) {
                pmm_free_page(page);
                return 0;
            }
        } else {
            int writable = (old_flags & PTE_W) || (requested & PTE_W);
            int executable = !(old_flags & PTE_NX) || !(requested & PTE_NX);
            if (writable && executable) {
                serial_write("EXEC: overlapping segments would violate W^X\n");
                return 0;
            }
            uint64_t merged = PTE_P | PTE_U | (writable ? PTE_W : 0) |
                              (executable ? 0 : PTE_NX);
            if (!paging_map_page_in(cr3, va, phys & ~(PAGE_SIZE - 1), merged)) return 0;
        }
    }
    return 1;
}

static int copy_from_file(vfs_node_t* node, uint64_t cr3,
                          const exec_segment_t* segment) {
    uint8_t buffer[512];
    uint64_t done = 0;
    while (done < segment->file_size) {
        uint64_t count = segment->file_size - done;
        if (count > sizeof(buffer)) count = sizeof(buffer);
        if (vfs_read(node, segment->file_offset + done, buffer, count) != (int64_t)count) return 0;
        for (uint64_t i = 0; i < count; i++) {
            uint64_t physical = paging_translate_in(cr3, segment->virtual_address + done + i, 0);
            if (!physical) return 0;
            *(uint8_t*)physical = buffer[i];
        }
        done += count;
    }
    return 1;
}

int exec_map_image(vfs_node_t* node, const exec_segment_t* segments,
                   uint32_t segment_count, uint64_t entry,
                   uint64_t* cr3_out, uint64_t* stack_top_out) {
    if (!node || !segments || !segment_count || segment_count > 128 ||
        entry < USER_MIN || entry >= USER_STACK_BASE) return 0;
    uint64_t cr3 = paging_create_user_address_space();
    if (!cr3) return 0;
    for (uint32_t i = 0; i < segment_count; i++) {
        const exec_segment_t* s = &segments[i];
        if (!s->memory_size || s->file_size > s->memory_size ||
            !add_ok(s->file_offset, s->file_size, node->size) ||
            s->virtual_address < USER_MIN ||
            !add_ok(s->virtual_address, s->memory_size, USER_TOP) ||
            s->virtual_address + s->memory_size > USER_STACK_BASE ||
            (s->flags & ~(EXEC_SEGMENT_READ|EXEC_SEGMENT_WRITE|EXEC_SEGMENT_EXEC)) ||
            ((s->flags & (EXEC_SEGMENT_WRITE|EXEC_SEGMENT_EXEC)) ==
             (EXEC_SEGMENT_WRITE|EXEC_SEGMENT_EXEC))) goto fail;
        if (!map_segment_pages(cr3, s->virtual_address,
                               s->virtual_address + s->memory_size, s->flags)) goto fail;
    }
    for (uint32_t i = 0; i < segment_count; i++)
        if (!copy_from_file(node, cr3, &segments[i])) goto fail;
    uint64_t entry_flags = 0;
    if (!paging_translate_in(cr3, entry, &entry_flags) || (entry_flags & PTE_NX)) goto fail;
    exec_segment_t stack = {0, USER_STACK_BASE, 0, USER_STACK_PAGES * PAGE_SIZE,
                            EXEC_SEGMENT_READ|EXEC_SEGMENT_WRITE};
    if (!map_segment_pages(cr3, stack.virtual_address,
                           stack.virtual_address + stack.memory_size, stack.flags)) goto fail;
    *cr3_out = cr3;
    *stack_top_out = USER_STACK_BASE + USER_STACK_PAGES * PAGE_SIZE;
    return 1;
fail:
    paging_destroy_user_address_space(cr3);
    return 0;
}

exec_format_t exec_detect(vfs_node_t* node) {
    uint8_t magic[4];
    if (!node || node->type != VFS_FILE ||
        vfs_read(node, 0, magic, sizeof(magic)) != (int64_t)sizeof(magic)) return EXEC_FORMAT_UNKNOWN;
    if (magic[0] == 0x7f && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F')
        return elf_probe(node) ? EXEC_FORMAT_ELF64 : EXEC_FORMAT_UNKNOWN;
    if (magic[0] == 'O' && magic[1] == 'B' && magic[2] == 'S' && magic[3] == 'X')
        return obsx_probe(node) ? EXEC_FORMAT_OBSIDIA_NATIVE : EXEC_FORMAT_UNKNOWN;
    if (magic[0] == 'M' && magic[1] == 'Z') {
        pe_image_info_t info;
        if (!pe_inspect(node, &info, 0, 0)) return EXEC_FORMAT_UNKNOWN;
        return info.optional_magic == PE_MAGIC_PE32_PLUS ? EXEC_FORMAT_PE32_PLUS : EXEC_FORMAT_PE32;
    }
    return EXEC_FORMAT_UNKNOWN;
}

const char* exec_format_name(exec_format_t format) {
    switch (format) {
        case EXEC_FORMAT_ELF64: return "ELF64";
        case EXEC_FORMAT_OBSIDIA_NATIVE: return "Obsidia native";
        case EXEC_FORMAT_PE32: return "PE32";
        case EXEC_FORMAT_PE32_PLUS: return "PE32+";
        default: return "unknown";
    }
}

int exec_load(vfs_node_t* node, exec_format_t format, uint64_t* cr3_out,
              uint64_t* entry_out, uint64_t* stack_top_out) {
    switch (format) {
        case EXEC_FORMAT_ELF64:
            return elf_load_process(node, cr3_out, entry_out, stack_top_out);
        case EXEC_FORMAT_OBSIDIA_NATIVE:
            return obsx_load_process(node, cr3_out, entry_out, stack_top_out);
        case EXEC_FORMAT_PE32_PLUS:
            return pe_load_process(node, cr3_out, entry_out, stack_top_out);
        case EXEC_FORMAT_PE32:
            serial_write("WINABI: PE32 x86 execution is recognized but not supported yet\n");
            return 0;
        default:
            serial_write("EXEC: unknown or malformed executable image\n");
            return 0;
    }
}
