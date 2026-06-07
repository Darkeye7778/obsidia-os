#include "vfs.h"
#include "../console/console.h"
#include "../memory/memory.h"
#include "../memory/heap.h"
#include "../initrd/initrd.h"  // for the raw address/size and oar structs (we'll share defines lightly)

#include <stddef.h>

#define OAR_MAGIC0 'O'
#define OAR_MAGIC1 'A'
#define OAR_MAGIC2 'R'
#define OAR_MAGIC3 '1'
#define OAR_VERSION 1
#define OAR_TYPE_FILE 1
#define OAR_TYPE_DIR  2

typedef struct __attribute__((packed)) {
    char magic[4];
    uint32_t version;
    uint32_t file_count;
    uint32_t flags;
} oar_header_t;

typedef struct __attribute__((packed)) {
    uint32_t name_len;
    uint32_t type;
    uint64_t size;
    uint64_t flags;
} oar_entry_t;

static vfs_node_t* vfs_root = 0;
static int vfs_mounted = 0;

static uint64_t align8(uint64_t v) { return (v + 7) & ~7; }

static int oar_valid(oar_header_t* h) {
    return h && h->magic[0]==OAR_MAGIC0 && h->magic[1]==OAR_MAGIC1 &&
           h->magic[2]==OAR_MAGIC2 && h->magic[3]==OAR_MAGIC3 &&
           h->version == OAR_VERSION;
}

void vfs_init(void) {
    vfs_root = 0;
    vfs_mounted = 0;
}

static vfs_node_t* create_node(const char* name, vfs_node_type_t type, uint64_t size, void* data_ptr) {
    vfs_node_t* n = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!n) return 0;

    // copy name safely
    int i = 0;
    for (; name[i] && i < 127; i++) n->name[i] = name[i];
    n->name[i] = 0;

    n->type = type;
    n->size = size;
    n->flags = 0;
    n->data = data_ptr;
    n->data_offset = 0;
    n->parent = 0;
    n->next_sibling = 0;
    n->children = 0;
    return n;
}

static void add_child(vfs_node_t* parent, vfs_node_t* child) {
    if (!parent || !child) return;
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;
}

int vfs_mount_initrd(void) {
    if (vfs_mounted) return 0;

    uint64_t addr = 0, sz = 0;
    // We access the raw initrd via the existing initrd module (it stores the base)
    // For simplicity we re-use the address that was set. We need a small accessor.
    // Since initrd.c keeps statics private, we will parse directly here using the module info
    // from main (but to avoid tight coupling, expose via a new small function or duplicate set).
    // Workaround: call into initrd to get raw info if we extend, or for now parse using
    // the same logic and assume the first module is still mapped. We will add a helper.

    // To keep changes minimal, we will directly access the raw bytes by calling a new
    // thin accessor we'll add to initrd.h/c in a moment. For this write, assume we can get it.

    // For the initial implementation we will parse using the initrd_set address if available.
    // We'll extend initrd slightly to expose the base for the VFS backend.

    // Placeholder: the actual mount will be called after initrd_set.
    // We implement the parse assuming we are given the address later or we read it.

    // To make it work now: we'll add a getter in initrd and call from here.
    // (We will edit initrd next.)

    vfs_mounted = 1; // will be set properly after parse
    return 1; // success indicator, real work in mount_from_addr
}

// We will provide a direct mount function that the kernel calls with the raw module.
int vfs_mount_initrd_from(uint64_t raw_addr, uint64_t raw_size) {
    if (!raw_addr || !raw_size || vfs_mounted) return 0;

    oar_header_t* header = (oar_header_t*)raw_addr;
    if (!oar_valid(header)) {
        console_print("VFS: Invalid OAR header\n");
        return 0;
    }

    // Create root dir
    vfs_root = create_node("/", VFS_DIR, 0, 0);
    if (!vfs_root) return 0;

    uint8_t* ptr = (uint8_t*)raw_addr + sizeof(oar_header_t);

    // First pass: create all nodes (flat for simplicity + basic children for dirs we see)
    // We build a flat list under root for now; subdir support can be added by splitting names.
    for (uint32_t i = 0; i < header->file_count; i++) {
        oar_entry_t* entry = (oar_entry_t*)ptr;
        ptr += sizeof(oar_entry_t);

        char namebuf[128];
        uint32_t nl = entry->name_len;
        if (nl >= 127) nl = 127;
        for (uint32_t j=0; j<nl; j++) namebuf[j] = ((char*)ptr)[j];
        namebuf[nl] = 0;

        ptr += entry->name_len;

        uint8_t* content = ptr;

        vfs_node_type_t t = (entry->type == OAR_TYPE_DIR) ? VFS_DIR : VFS_FILE;
        vfs_node_t* node = create_node(namebuf, t, entry->size, content);
        if (node) {
            // For dirs we could create hierarchy, but for base we attach all under root
            // (the builder puts "dir/" entries; GUI can filter or we can improve later)
            add_child(vfs_root, node);
        }

        ptr += entry->size;
        uint64_t off = (uint64_t)(ptr - (uint8_t*)raw_addr);
        ptr = (uint8_t*)raw_addr + align8(off);
    }

    vfs_mounted = 1;
    console_print("VFS: initrd mounted as root\n");
    return 1;
}

vfs_node_t* vfs_get_root(void) {
    return vfs_root;
}

vfs_node_t* vfs_find_child(vfs_node_t* dir, const char* name) {
    if (!dir || dir->type != VFS_DIR) return 0;
    vfs_node_t* c = dir->children;
    while (c) {
        // simple strcmp
        const char* a = c->name;
        const char* b = name;
        int eq = 1;
        while (*a && *b) { if (*a++ != *b++) { eq=0; break; } }
        if (eq && *a == 0 && *b == 0) return c;
        c = c->next_sibling;
    }
    return 0;
}

vfs_node_t* vfs_open(const char* path) {
    if (!path || !vfs_root) return 0;

    // Skip leading /
    while (*path == '/') path++;

    if (*path == 0) return vfs_root; // root itself

    // For base: only root level names supported (no /sub/file yet)
    // Find direct child
    return vfs_find_child(vfs_root, path);
}

int64_t vfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len) {
    if (!node || node->type != VFS_FILE || !buf || !node->data) return -1;
    if (offset >= node->size) return 0;

    uint64_t avail = node->size - offset;
    if (len > avail) len = avail;

    // Zero copy: the data pointer is into the initrd module memory
    const uint8_t* src = (const uint8_t*)node->data + offset;
    uint8_t* dst = (uint8_t*)buf;
    for (uint64_t i = 0; i < len; i++) dst[i] = src[i];

    return (int64_t)len;
}

void vfs_list(vfs_node_t* dir_node) {
    if (!dir_node || dir_node->type != VFS_DIR) {
        console_print("Not a directory\n");
        return;
    }
    vfs_node_t* c = dir_node->children;
    int count = 0;
    while (c) {
        console_print("  ");
        console_print(c->name);
        if (c->type == VFS_DIR) console_print(" <dir>");
        console_print("\n");
        c = c->next_sibling;
        count++;
    }
    if (count == 0) console_print("  (empty)\n");
}

void vfs_close(vfs_node_t* node) {
    (void)node;
    // For read-only in-memory nodes we do nothing (they live until unmount)
    // Future: refcounts, dynamic alloc for paths etc.
}