#include "vfs.h"
#include "../block.h"
#include "../console/console.h"
#include "../memory/heap.h"
#include "../timer.h"
#include <stdint.h>
#include <stddef.h>

#define STATEFS_VERSION_V1 1U
#define STATEFS_VERSION 2U
#define STATEFS_SLOT_SECTORS 128U
#define STATEFS_SLOT0_LBA 1U
#define STATEFS_SLOT1_LBA (STATEFS_SLOT0_LBA + STATEFS_SLOT_SECTORS)
#define STATEFS_PAYLOAD_MAX ((STATEFS_SLOT_SECTORS - 1U) * 512U)
#define STATEFS_MAX_V1_FILES 32U
#define STATEFS_MAX_ENTRIES 64U
#define STATEFS_MAX_FILE_SIZE 32768U
#define STATEFS_COMMITTED 0x434f4d4dU

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t header_size;
    uint64_t generation;
    uint32_t payload_size;
    uint32_t payload_crc;
    uint32_t entry_count;
    uint32_t committed;
    uint32_t header_crc;
    uint8_t reserved[512 - 44];
} statefs_header_t;

typedef struct {
    uint8_t* data;
    uint32_t capacity;
} statefs_file_t;

static block_device_t* disk;
static vfs_node_t* mount_node;
static uint64_t generation;
static uint32_t active_slot;

static uint32_t crc32(const void* data, uint32_t length) {
    uint32_t crc = 0xffffffffU;
    const uint8_t* bytes = data;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint32_t bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320U & ((uint32_t)-(int32_t)(crc & 1)));
    }
    return ~crc;
}

static int bytes_equal(const char* a, const char* b, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static uint32_t align4(uint32_t value) { return (value + 3U) & ~3U; }
static uint64_t slot_lba(uint32_t slot) { return slot ? STATEFS_SLOT1_LBA : STATEFS_SLOT0_LBA; }

static int valid_name(const char* name) {
    uint32_t length = 0;
    if (!name || !name[0]) return 0;
    for (; name[length]; length++)
        if (length >= 63 || (uint8_t)name[length] < 33 || name[length] == '/' || name[length] == '\\') return 0;
    if ((length == 1 && name[0] == '.') || (length == 2 && name[0] == '.' && name[1] == '.')) return 0;
    return 1;
}

static int header_valid(uint32_t slot, statefs_header_t* header, uint8_t* payload) {
    static const char magic_v1[8] = {'O','B','S','S','T','F','1',0};
    static const char magic_v2[8] = {'O','B','S','S','T','F','2',0};
    if (block_read(disk, slot_lba(slot), 1, header)) return 0;
    int v1 = header->version == STATEFS_VERSION_V1 && bytes_equal(header->magic, magic_v1, 8);
    int v2 = header->version == STATEFS_VERSION && bytes_equal(header->magic, magic_v2, 8);
    uint32_t limit = v1 ? STATEFS_MAX_V1_FILES : STATEFS_MAX_ENTRIES;
    if ((!v1 && !v2) || header->header_size != 512 || header->committed != STATEFS_COMMITTED ||
        header->payload_size > STATEFS_PAYLOAD_MAX || header->entry_count > limit ||
        crc32(header, 40) != header->header_crc) return 0;
    uint32_t sectors = (header->payload_size + 511U) / 512U;
    if (sectors && block_read(disk, slot_lba(slot) + 1, sectors, payload)) return 0;
    return crc32(payload, header->payload_size) == header->payload_crc;
}

static int detach_node(vfs_node_t* node) {
    if (!node || !node->parent) return -1;
    vfs_node_t** link = &node->parent->children;
    while (*link && *link != node) link = &(*link)->next_sibling;
    if (!*link) return -1;
    *link = node->next_sibling;
    node->next_sibling = 0;
    return 0;
}

static void free_node(vfs_node_t* node) {
    if (!node) return;
    if (node->type == VFS_FILE) {
        statefs_file_t* file = node->fs_data;
        if (file) {
            if (file->data) kfree(file->data);
            kfree(file);
        }
    }
    kfree(node);
}

static int statefs_sync(vfs_node_t* ignored);
static int statefs_rename(vfs_node_t* node, vfs_node_t* new_dir, const char* new_name, int replace);
static int statefs_unlink(vfs_node_t* node);

static int64_t statefs_read(vfs_node_t* node, uint64_t offset, void* buffer, uint64_t length) {
    statefs_file_t* file = node && node->type == VFS_FILE ? node->fs_data : 0;
    if (!file || offset >= node->size) return file ? 0 : -1;
    if (length > node->size - offset) length = node->size - offset;
    for (uint64_t i = 0; i < length; i++) ((uint8_t*)buffer)[i] = file->data[offset + i];
    return (int64_t)length;
}

static int64_t statefs_write(vfs_node_t* node, uint64_t offset, const void* buffer, uint64_t length) {
    statefs_file_t* file = node && node->type == VFS_FILE ? node->fs_data : 0;
    if (!file || length > STATEFS_MAX_FILE_SIZE || offset > STATEFS_MAX_FILE_SIZE ||
        length > STATEFS_MAX_FILE_SIZE - offset) return -1;
    uint32_t end = (uint32_t)(offset + length);
    if (end > file->capacity) {
        uint32_t capacity = 4096;
        while (capacity < end) capacity *= 2;
        uint8_t* data = kmalloc(capacity);
        if (!data) return -1;
        for (uint32_t i = 0; i < node->size; i++) data[i] = file->data[i];
        for (uint32_t i = (uint32_t)node->size; i < capacity; i++) data[i] = 0;
        if (file->data) kfree(file->data);
        file->data = data;
        file->capacity = capacity;
    }
    for (uint32_t i = 0; i < length; i++) file->data[offset + i] = ((const uint8_t*)buffer)[i];
    if (end > node->size) node->size = end;
    node->modified_ticks = timer_get_ticks();
    return (int64_t)length;
}

static vfs_ops_t statefs_ops;

static vfs_node_t* new_node(vfs_node_t* parent, const char* name, vfs_node_type_t type,
                            const uint8_t* data, uint32_t size, uint64_t created, uint64_t modified) {
    if (!parent || !valid_name(name) || (type != VFS_FILE && type != VFS_DIR) ||
        (type == VFS_FILE && size > STATEFS_MAX_FILE_SIZE)) return 0;
    vfs_node_t* node = kmalloc(sizeof(*node));
    if (!node) return 0;
    for (uint32_t i = 0; i < sizeof(*node); i++) ((uint8_t*)node)[i] = 0;
    uint32_t i = 0;
    for (; name[i] && i < 127; i++) node->name[i] = name[i];
    node->type = type;
    node->size = size;
    node->ops = &statefs_ops;
    node->created_ticks = created;
    node->modified_ticks = modified;
    if (type == VFS_FILE) {
        statefs_file_t* file = kmalloc(sizeof(*file));
        if (!file) { kfree(node); return 0; }
        file->capacity = size > 4096 ? align4(size) : 4096;
        file->data = kmalloc(file->capacity);
        if (!file->data) { kfree(file); kfree(node); return 0; }
        for (i = 0; i < size; i++) file->data[i] = data[i];
        for (; i < file->capacity; i++) file->data[i] = 0;
        node->fs_data = file;
    }
    if (vfs_attach_child(parent, node)) { free_node(node); return 0; }
    return node;
}

static uint32_t entry_count(vfs_node_t* directory) {
    uint32_t count = 0;
    for (vfs_node_t* node = directory->children; node; node = node->next_sibling) {
        count++;
        if (node->type == VFS_DIR) count += entry_count(node);
    }
    return count;
}

static vfs_node_t* statefs_create(vfs_node_t* directory, const char* name, vfs_node_type_t type) {
    if (!directory || directory->ops != &statefs_ops || directory->type != VFS_DIR ||
        !valid_name(name) || vfs_find_child(directory, name) || entry_count(mount_node) >= STATEFS_MAX_ENTRIES) return 0;
    uint64_t now = timer_get_ticks();
    vfs_node_t* node = new_node(directory, name, type, 0, 0, now, now);
    if (!node) return 0;
    if (statefs_sync(node)) { detach_node(node); free_node(node); return 0; }
    return node;
}

static void statefs_close(vfs_node_t* node) { (void)node; }

static vfs_ops_t statefs_ops = {
    .read = statefs_read,
    .write = statefs_write,
    .create = statefs_create,
    .sync = statefs_sync,
    .rename = statefs_rename,
    .unlink = statefs_unlink,
    .close = statefs_close
};

static int make_path(vfs_node_t* node, char* output, uint32_t* length) {
    if (!node || node == mount_node) { *length = 0; return 0; }
    uint32_t parent_length = 0;
    if (make_path(node->parent, output, &parent_length)) return -1;
    uint32_t name_length = 0;
    while (node->name[name_length]) name_length++;
    uint32_t need = parent_length + (parent_length ? 1U : 0U) + name_length;
    if (!name_length || need > 127) return -1;
    if (parent_length) output[parent_length++] = '/';
    for (uint32_t i = 0; i < name_length; i++) output[parent_length + i] = node->name[i];
    output[need] = 0;
    *length = need;
    return 0;
}

static int serialize_directory(vfs_node_t* directory, uint8_t* payload, uint32_t* at, uint32_t* count) {
    for (vfs_node_t* node = directory->children; node; node = node->next_sibling) {
        char path[128];
        uint32_t path_length = 0;
        if (*count >= STATEFS_MAX_ENTRIES || make_path(node, path, &path_length)) return -1;
        uint32_t bytes = node->type == VFS_FILE ? (uint32_t)node->size : 0;
        uint32_t need = 24U + align4(path_length) + bytes;
        if (need > STATEFS_PAYLOAD_MAX - *at) return -1;
        payload[*at] = (uint8_t)path_length;
        payload[*at + 1] = (uint8_t)node->type;
        payload[*at + 2] = payload[*at + 3] = 0;
        for (uint32_t b = 0; b < 4; b++) payload[*at + 4 + b] = (uint8_t)(bytes >> (b * 8));
        for (uint32_t b = 0; b < 8; b++) payload[*at + 8 + b] = (uint8_t)(node->created_ticks >> (b * 8));
        for (uint32_t b = 0; b < 8; b++) payload[*at + 16 + b] = (uint8_t)(node->modified_ticks >> (b * 8));
        *at += 24;
        for (uint32_t i = 0; i < align4(path_length); i++) payload[*at + i] = i < path_length ? (uint8_t)path[i] : 0;
        *at += align4(path_length);
        if (bytes) {
            statefs_file_t* file = node->fs_data;
            for (uint32_t i = 0; i < bytes; i++) payload[*at + i] = file->data[i];
            *at += bytes;
        }
        (*count)++;
        if (node->type == VFS_DIR && serialize_directory(node, payload, at, count)) return -1;
    }
    return 0;
}

static int serialize(uint8_t* payload, uint32_t* size, uint32_t* count) {
    *size = 0;
    *count = 0;
    return serialize_directory(mount_node, payload, size, count);
}

static int statefs_sync(vfs_node_t* ignored) {
    (void)ignored;
    uint8_t* payload = kmalloc(STATEFS_PAYLOAD_MAX);
    if (!payload) return -1;
    uint32_t size = 0, count = 0;
    if (serialize(payload, &size, &count)) { kfree(payload); return -1; }
    uint32_t next = active_slot ^ 1U;
    uint32_t sectors = (size + 511U) / 512U;
    for (uint32_t i = size; i < sectors * 512U; i++) payload[i] = 0;
    if ((sectors && block_write(disk, slot_lba(next) + 1, sectors, payload)) || block_flush(disk)) {
        kfree(payload); return -1;
    }
    statefs_header_t header = {0};
    static const char magic[8] = {'O','B','S','S','T','F','2',0};
    for (uint32_t i = 0; i < 8; i++) header.magic[i] = magic[i];
    header.version = STATEFS_VERSION;
    header.header_size = 512;
    header.generation = generation + 1;
    header.payload_size = size;
    header.payload_crc = crc32(payload, size);
    header.entry_count = count;
    header.committed = STATEFS_COMMITTED;
    header.header_crc = crc32(&header, 40);
    int result = block_write(disk, slot_lba(next), 1, &header) || block_flush(disk);
    if (!result) { generation = header.generation; active_slot = next; }
    kfree(payload);
    return result ? -1 : 0;
}

static int statefs_unlink(vfs_node_t* node) {
    if (!node || node == mount_node || node->open_refs || node->children) return -1;
    vfs_node_t* parent = node->parent;
    if (detach_node(node)) return -1;
    if (statefs_sync(parent)) {
        node->parent = parent;
        node->next_sibling = parent->children;
        parent->children = node;
        return -1;
    }
    free_node(node);
    return 0;
}

static int statefs_rename(vfs_node_t* node, vfs_node_t* new_dir, const char* new_name, int replace) {
    if (!node || node == mount_node || !new_dir || node->ops != &statefs_ops || new_dir->ops != &statefs_ops ||
        new_dir->type != VFS_DIR || !valid_name(new_name)) return -1;
    for (vfs_node_t* parent = new_dir; parent; parent = parent->parent) if (parent == node) return -1;
    vfs_node_t* old = vfs_find_child(new_dir, new_name);
    if (old == node) return 0;
    if (old && (!replace || old->open_refs || old->type != node->type || old->children)) return -1;

    vfs_node_t* prior_parent = node->parent;
    uint64_t prior_modified = node->modified_ticks;
    char prior_name[128];
    for (uint32_t i = 0; i < 128; i++) prior_name[i] = node->name[i];
    if (old && detach_node(old)) return -1;
    if (detach_node(node)) {
        if (old) { old->parent = new_dir; old->next_sibling = new_dir->children; new_dir->children = old; }
        return -1;
    }
    uint32_t i = 0;
    for (; i < 127 && new_name[i]; i++) node->name[i] = new_name[i];
    node->name[i] = 0;
    node->parent = new_dir;
    node->next_sibling = new_dir->children;
    new_dir->children = node;
    node->modified_ticks = timer_get_ticks();
    if (statefs_sync(node)) {
        detach_node(node);
        for (i = 0; i < 128; i++) node->name[i] = prior_name[i];
        node->parent = prior_parent;
        node->modified_ticks = prior_modified;
        node->next_sibling = prior_parent->children;
        prior_parent->children = node;
        if (old) { old->parent = new_dir; old->next_sibling = new_dir->children; new_dir->children = old; }
        return -1;
    }
    if (old) free_node(old);
    return 0;
}

static uint32_t read_u32(const uint8_t* input) {
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8) | ((uint32_t)input[2] << 16) | ((uint32_t)input[3] << 24);
}
static uint64_t read_u64(const uint8_t* input) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < 8; i++) value |= (uint64_t)input[i] << (i * 8);
    return value;
}

static int split_loaded_path(const char* path, vfs_node_t** parent, char* name) {
    if(!path||!path[0]||path[0]=='/')return-1;
    vfs_node_t*directory=mount_node;uint32_t at=0;
    for(;;){
        char component[64];uint32_t length=0;
        while(path[at]&&path[at]!='/'){if(length>=63)return-1;component[length++]=path[at++];}
        component[length]=0;if(!valid_name(component))return-1;
        if(!path[at]){for(uint32_t i=0;i<=length;i++)name[i]=component[i];*parent=directory;return 0;}
        at++;if(!path[at])return-1;
        directory=vfs_find_child(directory,component);
        if(!directory||directory->type!=VFS_DIR||directory->ops!=&statefs_ops)return-1;
    }
}

static int load_v1(const statefs_header_t* header, const uint8_t* payload) {
    uint32_t at = 0;
    for (uint32_t entry = 0; entry < header->entry_count; entry++) {
        if (at > header->payload_size || header->payload_size - at < 8) return -1;
        uint32_t name_length = payload[at];
        uint32_t bytes = read_u32(payload + at + 4);
        at += 8;
        if (!name_length || name_length > 63 || bytes > STATEFS_MAX_FILE_SIZE || align4(name_length) > header->payload_size - at) return -1;
        char name[64];
        for (uint32_t i = 0; i < name_length; i++) name[i] = (char)payload[at + i];
        name[name_length] = 0;
        at += align4(name_length);
        if (!valid_name(name) || bytes > header->payload_size - at || vfs_find_child(mount_node, name) ||
            !new_node(mount_node, name, VFS_FILE, payload + at, bytes, 0, 0)) return -1;
        at += bytes;
    }
    return at == header->payload_size ? 0 : -1;
}

static int load_v2(const statefs_header_t* header, const uint8_t* payload) {
    uint32_t at = 0;
    for (uint32_t entry = 0; entry < header->entry_count; entry++) {
        if (at > header->payload_size || header->payload_size - at < 24) return -1;
        uint32_t path_length = payload[at];
        vfs_node_type_t type = (vfs_node_type_t)payload[at + 1];
        uint32_t bytes = read_u32(payload + at + 4);
        uint64_t created = read_u64(payload + at + 8);
        uint64_t modified = read_u64(payload + at + 16);
        at += 24;
        if (!path_length || path_length > 127 || (type != VFS_FILE && type != VFS_DIR) ||
            (type == VFS_DIR && bytes) || bytes > STATEFS_MAX_FILE_SIZE || align4(path_length) > header->payload_size - at) return -1;
        char path[128];
        for (uint32_t i = 0; i < path_length; i++) path[i] = (char)payload[at + i];
        path[path_length] = 0;
        at += align4(path_length);
        vfs_node_t* parent = 0;
        char name[64];
        if (split_loaded_path(path, &parent, name) || vfs_find_child(parent, name) || bytes > header->payload_size - at ||
            !new_node(parent, name, type, payload + at, bytes, created, modified)) return -1;
        at += bytes;
    }
    return at == header->payload_size ? 0 : -1;
}

int vfs_mount_statefs(const char* path, block_device_t* device) {
    if (!vfs_get_root() || !path || !device || device->block_size != 512 ||
        device->block_count <= STATEFS_SLOT1_LBA + STATEFS_SLOT_SECTORS) return -1;
    while (*path == '/') path++;
    if (!valid_name(path) || vfs_find_child(vfs_get_root(), path)) return -1;
    disk = device;
    mount_node = kmalloc(sizeof(*mount_node));
    if (!mount_node) return -1;
    for (uint32_t i = 0; i < sizeof(*mount_node); i++) ((uint8_t*)mount_node)[i] = 0;
    uint32_t i = 0;
    for (; path[i]; i++) mount_node->name[i] = path[i];
    mount_node->type = VFS_DIR;
    mount_node->ops = &statefs_ops;
    mount_node->created_ticks = timer_get_ticks();
    mount_node->modified_ticks = mount_node->created_ticks;
    if (vfs_attach_child(vfs_get_root(), mount_node)) { kfree(mount_node); return -1; }

    uint8_t* payload = kmalloc(STATEFS_PAYLOAD_MAX);
    if (!payload) return -1;
    statefs_header_t first, second;
    int first_valid = header_valid(0, &first, payload);
    int second_valid = header_valid(1, &second, payload);
    if (!first_valid && !second_valid) {
        generation = 0;
        active_slot = 1;
        kfree(payload);
        if (statefs_sync(mount_node)) return -1;
        console_print("statefs: formatted persistent volume v2\n");
        return 0;
    }
    active_slot = second_valid && (!first_valid || second.generation > first.generation) ? 1 : 0;
    statefs_header_t* header = active_slot ? &second : &first;
    if (!header_valid(active_slot, header, payload)) { kfree(payload); return -1; }
    int loaded = header->version == STATEFS_VERSION_V1 ? load_v1(header, payload) : load_v2(header, payload);
    if (loaded) { kfree(payload); return -1; }
    generation = header->generation;
    kfree(payload);
    console_print(header->version == STATEFS_VERSION_V1 ?
        "statefs: mounted v1 volume (upgrade on commit)\n" : "statefs: mounted persistent volume v2\n");
    return 0;
}
