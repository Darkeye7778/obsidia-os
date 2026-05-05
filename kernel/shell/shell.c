#include "shell.h"
#include "../console/console.h"
#include "../drivers/framebuffer.h"

static int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(const unsigned char*)a - *(const unsigned char*)b;
}

static int starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return 1;
}

void shell_execute(const char* input) {
    if (input[0] == 0) {
        return;
    }

    if (strcmp(input, "help") == 0) {
        console_print("Commands:\n");
        console_print("  help     - list commands\n");
        console_print("  clear    - clear screen\n");
        console_print("  echo     - print text\n");
        console_print("  version  - show kernel version\n");
        console_print("  about    - show OS info\n");
        console_print("  meminfo  - memory status\n");
        return;
    }

    if (strcmp(input, "clear") == 0) {
        fb_clear(0x00202020);
        console_reset;
        return;
    }

    if (starts_with(input, "echo ")) {
        console_print(input + 5);
        console_putc('\n');
        return;
    }

    if (strcmp(input, "version") == 0) {
        console_print("Obsidia OS kernel v0.4\n");
        return;
    }

    if (strcmp(input, "about") == 0) {
        console_print("Obsidia OS: custom hobby kernel built from scratch.\n");
        return;
    }

    if (strcmp(input, "meminfo") == 0) {
        console_print("Memory manager not initialized yet.\n");
        return;
    }

    console_print("Unknown command: ");
    console_print(input);
    console_putc('\n');
}
