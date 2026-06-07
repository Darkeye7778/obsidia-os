#pragma once
#include <stdint.h>

typedef enum {
    VFS_FILE = 1,
    VFS_DIR = 2
} vfs_node_type_t;

typedef struct vfs_node vfs_node_t;

struct vfs_node {
    char name[128];
    vfs_node_type_t type;
    uint64_t size;
    uint64_t flags;

    // Backend specific
    void* data;          // for files: pointer to content in initrd (or copied)
    uint64_t data_offset; // or absolute addr

    vfs_node_t* parent;
    vfs_node_t* next_sibling;
    vfs_node_t* children;  // head of child list for dirs
};

void vfs_init(void);

// Mount the current initrd OAR as the root filesystem (read-only for now)
int vfs_mount_initrd(void);

// Mount directly from raw Limine module address (called from main with module info)
int vfs_mount_initrd_from(uint64_t raw_addr, uint64_t raw_size);

// Lookup a node by path (simple /name or name for root level for now)
vfs_node_t* vfs_open(const char* path);

// Read from a file node (offset based)
int64_t vfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len);

// List children of a dir node (simple, populates or iterates)
void vfs_list(vfs_node_t* dir_node);

// Close / release node (for future refcount)
void vfs_close(vfs_node_t* node);

// Get the root directory node
vfs_node_t* vfs_get_root(void);

// Utility: find a direct child by name in a dir
vfs_node_t* vfs_find_child(vfs_node_t* dir, const char* name);
