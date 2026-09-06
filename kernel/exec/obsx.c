#include "obsx.h"
#include "image.h"
#include "../memory/heap.h"

static const uint8_t obsx_magic[8] = {'O','B','S','X','\r','\n',0x1a,'\n'};

static int add_ok(uint64_t a, uint64_t b, uint64_t limit) {
    return a <= limit && b <= limit - a;
}

static int read_header(vfs_node_t* node, obsx_header_t* header) {
    if (!node || node->type != VFS_FILE || node->size < sizeof(*header) ||
        vfs_read(node, 0, header, sizeof(*header)) != (int64_t)sizeof(*header)) return 0;
    for (uint32_t i = 0; i < sizeof(obsx_magic); i++)
        if (header->magic[i] != obsx_magic[i]) return 0;
    if (header->version != OBSX_VERSION || header->header_size != sizeof(*header) ||
        header->architecture != OBSX_ARCH_X86_64 || !header->segment_count ||
        header->segment_count > OBSX_MAX_SEGMENTS ||
        header->segment_entry_size != sizeof(obsx_segment_t) || header->reserved ||
        !add_ok(header->segment_table_offset,
                (uint64_t)header->segment_count * sizeof(obsx_segment_t), node->size) ||
        (header->import_table_size &&
         !add_ok(header->import_table_offset, header->import_table_size, node->size))) return 0;
    return 1;
}

int obsx_probe(vfs_node_t* node) {
    obsx_header_t header;
    return read_header(node, &header);
}

int obsx_load_process(vfs_node_t* node, uint64_t* cr3_out,
                      uint64_t* entry_out, uint64_t* stack_top_out) {
    obsx_header_t header;
    if (!read_header(node, &header) || header.import_table_size) return 0;
    uint64_t bytes = (uint64_t)header.segment_count * sizeof(obsx_segment_t);
    obsx_segment_t* input = kmalloc(bytes);
    exec_segment_t* segments = kmalloc((uint64_t)header.segment_count * sizeof(exec_segment_t));
    if (!input || !segments) { if (input) kfree(input); if (segments) kfree(segments); return 0; }
    if (vfs_read(node, header.segment_table_offset, input, bytes) != (int64_t)bytes) goto fail;
    for (uint32_t i = 0; i < header.segment_count; i++) {
        if (input[i].reserved || !input[i].alignment ||
            (input[i].alignment & (input[i].alignment - 1)) ||
            input[i].alignment > 0x200000ULL) goto fail;
        segments[i] = (exec_segment_t){input[i].file_offset, input[i].virtual_address,
            input[i].file_size, input[i].memory_size, input[i].flags};
    }
    if (!exec_map_image(node, segments, header.segment_count, header.entry_point,
                        cr3_out, stack_top_out)) goto fail;
    *entry_out = header.entry_point;
    kfree(input); kfree(segments); return 1;
fail:
    kfree(input); kfree(segments); return 0;
}
