#include "vfs.h"
#include "console/console.h"
#include "memory/heap.h"
#include "timer.h"
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
    if(len>UINT64_MAX-offset)return-1;
    uint64_t new_size = offset + len;
    if (new_size > f->capacity) {
        uint64_t new_cap = new_size + 4096;
        uint8_t* newd = (uint8_t*)kmalloc(new_cap);
        if (!newd) return -1;
        for (uint64_t i=0; i<node->size; i++) newd[i] = f->data[i];
        if(f->data)kfree(f->data);
        f->data = newd;
        f->capacity = new_cap;
    }
    for (uint64_t i=0; i<len; i++) f->data[offset + i] = ((const uint8_t*)buf)[i];
    if (new_size > node->size) node->size = new_size;
    node->modified_ticks = timer_get_ticks();
    return len;
}

static vfs_node_t* ramfs_create(vfs_node_t* dir, const char* name, vfs_node_type_t type) {
    if (!dir || dir->type != VFS_DIR || (type != VFS_FILE && type != VFS_DIR) || !name || !name[0]) return 0;
    uint32_t name_length=0;for(;name[name_length];name_length++)if(name_length>=127||name[name_length]=='/'||name[name_length]=='\\')return 0;
    if((name_length==1&&name[0]=='.')||(name_length==2&&name[0]=='.'&&name[1]=='.')||vfs_find_child(dir,name))return 0;
    vfs_node_t* n = (vfs_node_t*)kmalloc(sizeof(vfs_node_t));
    if (!n) return 0;
    for(uint32_t j=0;j<sizeof(*n);j++)((uint8_t*)n)[j]=0;
    int i=0; for(; name[i] && i<127; i++) n->name[i]=name[i]; n->name[i]=0;
    n->type = type;
    n->size = 0;
    n->flags = 0;
    n->created_ticks = timer_get_ticks();
    n->modified_ticks = n->created_ticks;
    n->open_refs = 0;
    n->ops = dir->ops; // inherit
    if (type == VFS_FILE) {
        ramfs_file_t* f = (ramfs_file_t*)kmalloc(sizeof(ramfs_file_t));
        if(!f){kfree(n);return 0;}
        f->data = (uint8_t*)kmalloc(4096);
        if(!f->data){kfree(f);kfree(n);return 0;}
        f->capacity = 4096;
        n->fs_data = f;
    } else {
        n->fs_data = 0;
    }
    if(vfs_attach_child(dir,n)){if(type==VFS_FILE){ramfs_file_t*f=n->fs_data;kfree(f->data);kfree(f);}kfree(n);return 0;}
    return n;
}

static void ramfs_close(vfs_node_t* node) { (void)node; }

static void ramfs_free_node(vfs_node_t*node){if(!node)return;if(node->type==VFS_FILE){ramfs_file_t*f=node->fs_data;if(f){if(f->data)kfree(f->data);kfree(f);}}kfree(node);}
static int ramfs_detach(vfs_node_t*node){if(!node||!node->parent)return-1;vfs_node_t**link=&node->parent->children;while(*link&&*link!=node)link=&(*link)->next_sibling;if(!*link)return-1;*link=node->next_sibling;node->next_sibling=0;return 0;}
static int ramfs_unlink(vfs_node_t*node){if(!node||node->open_refs||node->children||ramfs_detach(node))return-1;ramfs_free_node(node);return 0;}
static int ramfs_rename(vfs_node_t*node,vfs_node_t*new_dir,const char*new_name,int replace){
    if(!node||!new_dir||node->ops!=new_dir->ops||new_dir->type!=VFS_DIR||!new_name||!new_name[0])return-1;
    uint32_t length=0;for(;new_name[length];length++)if(length>=127||new_name[length]=='/'||new_name[length]=='\\')return-1;
    if((length==1&&new_name[0]=='.')||(length==2&&new_name[0]=='.'&&new_name[1]=='.'))return-1;
    for(vfs_node_t*p=new_dir;p;p=p->parent)if(p==node)return-1;
    vfs_node_t*old=vfs_find_child(new_dir,new_name);if(old==node)return 0;if(old&&(!replace||old->open_refs||old->type!=node->type||old->children))return-1;
    if(old){if(ramfs_detach(old))return-1;ramfs_free_node(old);}if(ramfs_detach(node))return-1;
    for(uint32_t i=0;i<128;i++)node->name[i]=0;for(uint32_t i=0;i<length;i++)node->name[i]=new_name[i];node->parent=new_dir;node->next_sibling=new_dir->children;new_dir->children=node;node->modified_ticks=timer_get_ticks();return 0;
}

static vfs_ops_t ramfs_ops = {
    .read = ramfs_read,
    .write = ramfs_write,
    .create = ramfs_create,
    .sync = 0,
    .rename = ramfs_rename,
    .unlink = ramfs_unlink,
    .close = ramfs_close
};

int vfs_mount_ramfs(const char* path) {
    if(!vfs_get_root()||!path)return-1;while(*path=='/')path++;if(!*path)return-1;
    char name[128];int i=0;while(path[i]&&path[i]!='/'&&i<127){name[i]=path[i];i++;}name[i]=0;if(path[i])return-1;
    vfs_node_t* tmp=(vfs_node_t*)kmalloc(sizeof(*tmp));if(!tmp)return-1;for(uint64_t j=0;j<sizeof(*tmp);j++)((uint8_t*)tmp)[j]=0;
    for(i=0;name[i];i++)tmp->name[i]=name[i];tmp->type=VFS_DIR;tmp->ops=&ramfs_ops;
    if(vfs_attach_child(vfs_get_root(),tmp)){kfree(tmp);return-1;}console_print("ramfs mounted at /tmp (writable)\n");return 0;
}
