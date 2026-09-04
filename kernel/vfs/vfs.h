#pragma once
#include <stdint.h>

typedef enum {
    VFS_FILE = 1,
    VFS_DIR = 2
} vfs_node_type_t;

struct vfs_node;
typedef struct vfs_node vfs_node_t;

typedef struct vfs_ops {
    int64_t (*read)(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len);
    int64_t (*write)(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t len);
    vfs_node_t* (*create)(vfs_node_t* dir, const char* name, vfs_node_type_t type);
    void (*close)(vfs_node_t* node);
} vfs_ops_t;

struct vfs_node {
    char name[128];
    vfs_node_type_t type;
    uint64_t size;
    uint64_t flags;

    vfs_ops_t* ops;      // per-FS operations
    void* fs_data;       // backend private (ramfs data, block dev, etc)

    vfs_node_t* parent;
    vfs_node_t* next_sibling;
    vfs_node_t* children;  // head of child list for dirs
};
typedef struct open_file { vfs_node_t* node; uint64_t offset; uint32_t flags; uint32_t refs; } open_file_t;
#define VFS_OPEN_CREATE 1U
#define VFS_OPEN_TRUNC  2U
open_file_t* vfs_open_file(const char* path,uint32_t flags);
void vfs_file_retain(open_file_t* file);
void vfs_file_release(open_file_t* file);
int64_t vfs_file_read(open_file_t* file,void* buf,uint64_t len);
int64_t vfs_file_write(open_file_t* file,const void* buf,uint64_t len);

void vfs_init(void);

// Mount initrd (read-only OAR)
int vfs_mount_initrd_from(uint64_t raw_addr, uint64_t raw_size);

// Mount a ramfs at given path (e.g. "/tmp")
int vfs_mount_ramfs(const char* path);

// Lookup
vfs_node_t* vfs_open(const char* path);

// Read / Write
int64_t vfs_read(vfs_node_t* node, uint64_t offset, void* buf, uint64_t len);
int64_t vfs_write(vfs_node_t* node, uint64_t offset, const void* buf, uint64_t len);

// List / create
void vfs_list(vfs_node_t* dir_node);
vfs_node_t* vfs_create(vfs_node_t* dir, const char* name, vfs_node_type_t type);

void vfs_close(vfs_node_t* node);
vfs_node_t* vfs_get_root(void);
vfs_node_t* vfs_find_child(vfs_node_t* dir, const char* name);
int vfs_attach_child(vfs_node_t* dir,vfs_node_t* child);

// For shell/debug
void vfs_list_mounts(void);
