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
