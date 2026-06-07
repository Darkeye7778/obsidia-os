#include "initrd.h"
#include "../console/console.h"
#include "../memory/memory.h"

static uint64_t initrd_address = 0;
static uint64_t initrd_size = 0;

void initrd_set(uint64_t address, uint64_t size) {
    initrd_address = address;
    initrd_size = size;
}

void initrd_print_info() {
    if (initrd_address == 0 || initrd_size == 0) {
	console_print("No initrd loaded.\n");
	return;
    }

    console_print("Initrd loaded.\n");

    console_print("Address: ");
    memory_print_hex64(initrd_address);
    console_print("\n");

    console_print("Size: ");
    memory_print_dec(initrd_size);
    console_print(" bytes\n");
}


#define OAR_TYPE_FILE 1
#define OAR_TYPE_DIR  2
#define PACKED __attribute__((packed))

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

static uint64_t align8(uint64_t value) {
    return (value + 7) & ~7;
}

static int oar_valid(oar_header_t* header) {
    return header->magic[0] == 'O' &&
           header->magic[1] == 'A' &&
           header->magic[2] == 'R' &&
           header->magic[3] == '1' &&
           header->version == 1;
}

static int str_equal_len(const char* a, const char* b, uint32_t b_len) {
    uint32_t i = 0;

    for (; i < b_len; i++) {
        if (a[i] == '\0') {
            return 0;
        }

        if (a[i] != b[i]) {
            return 0;
        }
    }

    return a[i] == '\0';
}

void initrd_list_files(void) {
    if (initrd_address == 0 || initrd_size == 0) {
        console_print("No initrd loaded.\n");
        return;
    }

    oar_header_t* header = (oar_header_t*)initrd_address;

    if (!oar_valid(header)) {
        console_print("Invalid OAR archive.\n");
        return;
    }

    uint8_t* ptr = (uint8_t*)initrd_address + sizeof(oar_header_t);

    console_print("OAR files:\n");

    for (uint32_t i = 0; i < header->file_count; i++) {
        oar_entry_t* entry = (oar_entry_t*)ptr;
        ptr += sizeof(oar_entry_t);

        char* name = (char*)ptr;

        console_print("  ");

        for (uint32_t j = 0; j < entry->name_len; j++) {
            console_putc(name[j]);
        }

        if (entry->type == OAR_TYPE_DIR) {
            console_print(" <dir>");
        }

        console_print("\n");

        ptr += entry->name_len;
        ptr += entry->size;

        uint64_t offset = (uint64_t)(ptr - (uint8_t*)initrd_address);
        ptr = (uint8_t*)initrd_address + align8(offset);
    }
}

void initrd_cat_file(const char* filename) {
    if (initrd_address == 0 || initrd_size == 0) {
        console_print("No initrd loaded.\n");
        return;
    }

    if (!filename || filename[0] == '\0') {
        console_print("Usage: cat <file>\n");
        return;
    }

    oar_header_t* header = (oar_header_t*)initrd_address;

    if (!oar_valid(header)) {
        console_print("Invalid OAR archive.\n");
        return;
    }

    uint8_t* ptr = (uint8_t*)initrd_address + sizeof(oar_header_t);

    for (uint32_t i = 0; i < header->file_count; i++) {
        oar_entry_t* entry = (oar_entry_t*)ptr;
        ptr += sizeof(oar_entry_t);

        char* name = (char*)ptr;
        ptr += entry->name_len;

        uint8_t* data = ptr;

        if (entry->type == OAR_TYPE_FILE &&
            str_equal_len(filename, name, entry->name_len)) {

            for (uint64_t j = 0; j < entry->size; j++) {
                console_putc(((char*)data)[j]);
            }

            console_print("\n");
            return;
        }

        ptr += entry->size;

        uint64_t offset = (uint64_t)(ptr - (uint8_t*)initrd_address);
        ptr = (uint8_t*)initrd_address + align8(offset);
    }

    console_print("File not found.\n");
}

uint64_t initrd_get_raw_addr(void) {
    return initrd_address;
}

uint64_t initrd_get_raw_size(void) {
    return initrd_size;
}
