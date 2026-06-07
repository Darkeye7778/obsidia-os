#include "vfs.h"
#include "../console/console.h"
#include "../memory/heap.h"
#include "../initrd/initrd.h"
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

// Default ops for initrd (read-only)
static int64_t initrd_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len);
static void initrd_close(vfs_node_t* node);

static vfs_ops_t initrd_ops = {
    .read = initrd_read,
    .write = 0,
    .create = 0,
    .close = initrd_close
};

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

static vfs_node_t* create_node(const char* name, vfs_node_type_t type, uint64_t size, void* data_ptr, vfs_ops_t* ops, void* fs_data) {
    vfs_node_t* n = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!n) return 0;
    int i=0; for (; name[i] && i<127; i++) n->name[i] = name[i]; n->name[i]=0;
    n->type = type;
    n->size = size;
    n->flags = 0;
    n->ops = ops;
    n->fs_data = fs_data;
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

int vfs_mount_initrd_from(uint64_t raw_addr, uint64_t raw_size) {
    if (!raw_addr || !raw_size || vfs_mounted) return 0;

    oar_header_t* header = (oar_header_t*)raw_addr;
    if (!oar_valid(header)) {
        console_print("VFS: Invalid OAR header\n");
        return 0;
    }

    vfs_root = create_node("/", VFS_DIR, 0, 0, &initrd_ops, 0);
    if (!vfs_root) return 0;

    uint8_t* ptr = (uint8_t*)raw_addr + sizeof(oar_header_t);

    for (uint32_t i = 0; i < header->file_count; i++) {
        oar_entry_t* entry = (oar_entry_t*)ptr;
        ptr += sizeof(oar_entry_t);

        char namebuf[128];
        uint32_t nl = entry->name_len; if (nl >= 127) nl=127;
        for (uint32_t j=0; j<nl; j++) namebuf[j] = ((char*)ptr)[j];
        namebuf[nl] = 0;
        ptr += entry->name_len;

        uint8_t* content = ptr;

        vfs_node_type_t t = (entry->type == OAR_TYPE_DIR) ? VFS_DIR : VFS_FILE;
        vfs_node_t* node = create_node(namebuf, t, entry->size, 0, &initrd_ops, content);
        if (node) add_child(vfs_root, node);

        ptr += entry->size;
        ptr = (uint8_t*)raw_addr + align8((uint64_t)(ptr - (uint8_t*)raw_addr));
    }

    vfs_mounted = 1;
    console_print("VFS: initrd mounted as root (read-only)\n");
    return 1;
}

vfs_node_t* vfs_get_root(void) { return vfs_root; }

vfs_node_t* vfs_find_child(vfs_node_t* dir, const char* name) {
    if (!dir || dir->type != VFS_DIR) return 0;
    vfs_node_t* c = dir->children;
    while (c) {
        const char* a = c->name; const char* b = name; int eq=1;
        while (*a && *b) { if (*a++ != *b++) {eq=0;break;} }
        if (eq && *a==0 && *b==0) return c;
        c = c->next_sibling;
    }
    return 0;
}

vfs_node_t* vfs_open(const char* path) {
    if (!path || !vfs_root) return 0;
    while (*path == '/') path++;
    if (*path == 0) return vfs_root;
    return vfs_find_child(vfs_root, path);  // flat for now
}

int64_t vfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len) {
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, offset, buf, len);
}

int64_t vfs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t len) {
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, offset, buf, len);
}

void vfs_list(vfs_node_t* dir_node) {
    if (!dir_node || dir_node->type != VFS_DIR) { console_print("Not a directory\n"); return; }
    vfs_node_t* c = dir_node->children; int count=0;
    while (c) {
        console_print("  "); console_print(c->name);
        if (c->type == VFS_DIR) console_print(" <dir>");
        console_print("\n"); c = c->next_sibling; count++;
    }
    if (count==0) console_print("  (empty)\n");
}

vfs_node_t* vfs_create(vfs_node_t* dir, const char* name, vfs_node_type_t type) {
    if (!dir || !dir->ops || !dir->ops->create) return 0;
    return dir->ops->create(dir, name, type);
}

void vfs_close(vfs_node_t* node) {
    if (node && node->ops && node->ops->close) node->ops->close(node);
}

void vfs_list_mounts(void) {
    console_print("Mounts:\n  / (initrd/OAR read-only)\n  /tmp (ramfs writable if mounted)\n");
}

// initrd read impl (from old data pointer)
static int64_t initrd_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len) {
    if (!node || node->type != VFS_FILE) return -1;
    uint8_t* content = (uint8_t*)node->fs_data;
    if (!content) return -1;
    if (offset >= node->size) return 0;
    uint64_t avail = node->size - offset; if (len > avail) len = avail;
    for (uint64_t i=0; i<len; i++) ((uint8_t*)buf)[i] = content[offset + i];
    return len;
}

static void initrd_close(vfs_node_t* node) { (void)node; }

// ramfs mount implemented in ramfs.c , called from main or shell
// For now, expose via vfs_mount_ramfs which is in ramfs.c (linked)
