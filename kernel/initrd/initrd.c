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

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
};

static uint64_t octal_to_int(const char* str, int size) {
    uint64_t value = 0;

    for (int i = 0; i < size; i++) {
	if (str[i] < '0' || str[i] > '7') {
	    break;
	}

	value = (value * 8) + (str[i] - '0');
    }

    return value;
}

void initrd_list_files(void) {
    if (initrd_address == 0) {
        console_print("No initrd loaded.\n");
        return;
    }

    uint8_t* ptr = (uint8_t*)initrd_address;

    console_print("Initrd files:\n");

    while (1) {
        struct tar_header* header = (struct tar_header*)ptr;

        // End of archive
        if (header->name[0] == '\0') {
            break;
        }

        console_print("  ");
        console_print(header->name);
        console_print("\n");

        uint64_t size = octal_to_int(header->size, 11);

        // Move to next TAR entry
        uint64_t offset = ((size + 511) / 512) * 512;
        ptr += 512 + offset;
    }
}

void initrd_cat_file(const char* filename) {
    if (initrd_address == 0) {
        console_print("No initrd loaded.\n");
        return;
    }

    uint8_t* ptr = (uint8_t*)initrd_address;

    while (1) {
        struct tar_header* header = (struct tar_header*)ptr;

        // End of archive
        if (header->name[0] == '\0') {
            break;
        }

        uint64_t size = octal_to_int(header->size, 11);

        // Compare filenames
        int match = 1;
        for (int i = 0;; i++) {
            if (filename[i] != header->name[i]) {
                match = 0;
                break;
            }

            if (filename[i] == '\0') {
                break;
            }
        }

        if (match) {
            char* file_data = (char*)(ptr + 512);

            console_print(file_data);
            console_print("\n");

            return;
        }

        uint64_t offset = ((size + 511) / 512) * 512;
        ptr += 512 + offset;
    }

    console_print("File not found.\n");
}
