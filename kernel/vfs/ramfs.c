#include "vfs.h"
#include "console/console.h"
#include "memory/heap.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ramfs_file {
    uint8_t* data;
    uint64_t capacity;
} ramfs_file_t;

static int64_t ramfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len) {
    ramfs_file_t* f = (ramfs_file_t*)node->fs_data;
    if (!f || !f->data) return -1;
    if (offset >= node->size) return 0;
    uint64_t avail = node->size - offset;
    if (len > avail) len = avail;
    for (uint64_t i=0; i<len; i++) ((uint8_t*)buf)[i] = f->data[offset + i];
    return len;
}

static int64_t ramfs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t len) {
    ramfs_file_t* f = (ramfs_file_t*)node->fs_data;
    if (!f) return -1;
    uint64_t new_size = offset + len;
    if (new_size > f->capacity) {
        uint64_t new_cap = new_size + 4096;
        uint8_t* newd = (uint8_t*)kmalloc(new_cap);
        if (!newd) return -1;
        for (uint64_t i=0; i<node->size; i++) newd[i] = f->data[i];
        // kfree old if wanted
        f->data = newd;
        f->capacity = new_cap;
    }
    for (uint64_t i=0; i<len; i++) f->data[offset + i] = ((const uint8_t*)buf)[i];
    if (new_size > node->size) node->size = new_size;
    return len;
}

static vfs_node_t* ramfs_create(vfs_node_t* dir, const char* name, vfs_node_type_t type) {
    if (!dir || dir->type != VFS_DIR) return 0;
    vfs_node_t* n = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!n) return 0;
    int i=0; for(; name[i] && i<127; i++) n->name[i]=name[i]; n->name[i]=0;
    n->type = type;
    n->size = 0;
    n->flags = 0;
    n->ops = dir->ops; // inherit
    n->parent = dir;
    n->next_sibling = dir->children;
    dir->children = n;
    n->children = 0;
    if (type == VFS_FILE) {
        ramfs_file_t* f = (ramfs_file_t*)kmalloc(sizeof(ramfs_file_t));
        f->data = (uint8_t*)kmalloc(4096);
        f->capacity = 4096;
        n->fs_data = f;
    } else {
        n->fs_data = 0;
    }
    return n;
}

static void ramfs_close(vfs_node_t* node) { (void)node; }

static vfs_ops_t ramfs_ops = {
    .read = ramfs_read,
    .write = ramfs_write,
    .create = ramfs_create,
    .close = ramfs_close
};

int vfs_mount_ramfs(const char* path) {
    // For simplicity, mount at root children or replace root for /tmp like, but since flat, create dir and attach ops
    // To keep simple for phase 2: if path is "/", or create "tmp" dir with ramfs ops.
    if (!vfs_get_root()) return -1;
    vfs_node_t* tmp = vfs_create(vfs_get_root(), "tmp", VFS_DIR);
    if (tmp) {
        tmp->ops = &ramfs_ops;
        console_print("ramfs mounted at /tmp (writable)\n");
        return 0;
    }
    return -1;
}
