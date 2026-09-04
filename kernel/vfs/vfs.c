#include "vfs.h"
#include "../memory/heap.h"
#include "../console/console.h"
#include "../initrd/initrd.h"
#include <stddef.h>

#define OAR_MAGIC0 'O'
#define OAR_MAGIC1 'A'
#define OAR_MAGIC2 'R'
#define OAR_MAGIC3 '1'
#define OAR_VERSION 1
#define OAR_TYPE_FILE 1
#define OAR_TYPE_DIR  2

extern void serial_write(const char* s);

static void serial_write_hex8(uint8_t v) {
    const char* h = "0123456789ABCDEF";
    char out[3];
    out[0] = h[(v >> 4) & 0xF];
    out[1] = h[v & 0xF];
    out[2] = 0;
    serial_write(out);
}

static void serial_write_u64(uint64_t value) {
    char buf[21];
    int i = 20;
    buf[i] = '\0';

    if (value == 0) {
        serial_write("0");
        return;
    }

    while (value > 0 && i > 0) {
        buf[--i] = '0' + (value % 10);
        value /= 10;
    }

    serial_write(&buf[i]);
}

static void serial_write_hex64(uint64_t value) {
    const char* hex = "0123456789ABCDEF";
    serial_write("0x");

    for (int i = 60; i >= 0; i -= 4) {
        uint8_t nibble = (value >> i) & 0xF;
        char c[2];
        c[0] = hex[nibble];
        c[1] = '\0';
        serial_write(c);
    }
}

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
int vfs_attach_child(vfs_node_t* dir,vfs_node_t* child){if(!dir||dir->type!=VFS_DIR||!child)return-1;if(vfs_find_child(dir,child->name))return-1;add_child(dir,child);return 0;}

int vfs_mount_initrd_from(uint64_t raw_addr, uint64_t raw_size) {
    serial_write("VFS mount: raw_addr=");
    serial_write("present");
    serial_write(" raw_size=");
    serial_write("present\n");

    if (!raw_addr || raw_size < sizeof(oar_header_t) || raw_addr > UINT64_MAX-raw_size) {
        serial_write("VFS mount fail: missing addr/size\n");
        return 0;
    }

    if (vfs_mounted) {
        serial_write("VFS mount fail: already mounted\n");
        return 0;
    }

    oar_header_t* header = (oar_header_t*)raw_addr;

    serial_write("OAR magic bytes: ");
    serial_write_hex8((uint8_t)header->magic[0]); serial_write(" ");
    serial_write_hex8((uint8_t)header->magic[1]); serial_write(" ");
    serial_write_hex8((uint8_t)header->magic[2]); serial_write(" ");
    serial_write_hex8((uint8_t)header->magic[3]); serial_write("\n");

    if (!oar_valid(header)) {
        serial_write("VFS mount fail: invalid OAR header\n");
        console_print("VFS: Invalid OAR header\n");
        return 0;
    }
    serial_write("VFS: OAR valid\n");
    serial_write("VFS: file count=");
    serial_write_u64(header->file_count);
    serial_write("\n");
    
    vfs_root = create_node("/", VFS_DIR, 0, 0, &initrd_ops, 0);

    if (!vfs_root) {
        serial_write("VFS FAIL: could not allocate root node\n");
        return 0;
    }
    
    serial_write("VFS: root node created\n");
    
    uint8_t* ptr = (uint8_t*)raw_addr + sizeof(oar_header_t);

    serial_write("VFS: starting entry parse\n");

    uint8_t* end=(uint8_t*)raw_addr+raw_size;
    for (uint32_t i = 0; i < header->file_count; i++) {
        if(ptr>end||sizeof(oar_entry_t)>(uint64_t)(end-ptr)){serial_write("VFS: truncated entry header\n");return 0;}
        oar_entry_t* entry = (oar_entry_t*)ptr;

        serial_write("VFS: found entry\n");

        ptr += sizeof(oar_entry_t);

        if(entry->name_len==0||entry->name_len>=128||entry->name_len>(uint64_t)(end-ptr)){serial_write("VFS: invalid entry name\n");return 0;}

        char namebuf[128];
        uint32_t nl = entry->name_len; if (nl >= 127) nl=127;
        for (uint32_t j=0; j<nl; j++) namebuf[j] = ((char*)ptr)[j];
        namebuf[nl] = 0;
        serial_write("VFS entry name: ");
        serial_write(namebuf);
        serial_write("\n");
        ptr += entry->name_len;

        if(entry->size>(uint64_t)(end-ptr)){serial_write("VFS: truncated entry data\n");return 0;}

        uint8_t* content = ptr;

        vfs_node_type_t t = (entry->type == OAR_TYPE_DIR) ? VFS_DIR : VFS_FILE;
        vfs_node_t* node = create_node(namebuf, t, entry->size, 0, &initrd_ops, content);
        if (node) add_child(vfs_root, node);

        ptr += entry->size;
        uint64_t used=(uint64_t)(ptr-(uint8_t*)raw_addr);if(used>UINT64_MAX-7||align8(used)>raw_size){serial_write("VFS: invalid entry alignment\n");return 0;}ptr=(uint8_t*)raw_addr+align8(used);

        serial_write("vfs_root ptr=");
        serial_write_hex64((uint64_t)vfs_root);
        serial_write("\n");
        
        serial_write("first child ptr=");
        serial_write_hex64((uint64_t)vfs_root->children);
        serial_write("\n");
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
    vfs_node_t* node=vfs_root;while(*path=='/')path++;
    while(*path){char component[128];uint32_t n=0;while(*path&&*path!='/'){if(n>=127)return 0;component[n++]=*path++;}component[n]=0;while(*path=='/')path++;
        if(n==1&&component[0]=='.')continue;
        if(n==2&&component[0]=='.'&&component[1]=='.'){if(node->parent)node=node->parent;continue;}
        node=vfs_find_child(node,component);if(!node)return 0;
    }return node;
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

static vfs_node_t* create_path_file(const char* path){char parent_path[128],name[128];uint32_t length=0;while(path[length]){if(length>=127)return 0;length++;}while(length&&path[length-1]=='/')length--;if(!length)return 0;uint32_t split=length;while(split&&path[split-1]!='/')split--;uint32_t nn=length-split;if(!nn||nn>=128)return 0;for(uint32_t i=0;i<nn;i++)name[i]=path[split+i];name[nn]=0;if(split==0){parent_path[0]='/';parent_path[1]=0;}else{uint32_t pn=split;while(pn>1&&path[pn-1]=='/')pn--;for(uint32_t i=0;i<pn;i++)parent_path[i]=path[i];parent_path[pn]=0;}vfs_node_t*parent=vfs_open(parent_path);if(!parent||parent->type!=VFS_DIR)return 0;return vfs_create(parent,name,VFS_FILE);}
open_file_t* vfs_open_file(const char* path,uint32_t flags){vfs_node_t*n=vfs_open(path);if(!n&&(flags&VFS_OPEN_CREATE))n=create_path_file(path);if(!n||n->type!=VFS_FILE)return 0;if((flags&VFS_OPEN_TRUNC)&&n->ops&&n->ops->write)n->size=0;open_file_t*f=kmalloc(sizeof(*f));if(!f)return 0;*f=(open_file_t){n,0,flags,1};return f;}
void vfs_file_retain(open_file_t*f){if(f)f->refs++;}
void vfs_file_release(open_file_t*f){if(f&&f->refs&&!--f->refs){vfs_close(f->node);kfree(f);}}
int64_t vfs_file_read(open_file_t*f,void*b,uint64_t n){if(!f)return-1;int64_t r=vfs_read(f->node,f->offset,b,n);if(r>0)f->offset+=(uint64_t)r;return r;}
int64_t vfs_file_write(open_file_t*f,const void*b,uint64_t n){if(!f)return-1;int64_t r=vfs_write(f->node,f->offset,b,n);if(r>0)f->offset+=(uint64_t)r;return r;}

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
