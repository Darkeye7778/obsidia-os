#include "shell.h"
#include "../console/console.h"
#include "../drivers/framebuffer.h"
#include "../memory/memory.h"
#include <stddef.h>

typedef void (*command_func_t)(const char *args);

typedef struct {
    const char *name;
    const char *description;
    command_func_t func;
} command_t;

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

static void cmd_help(const char* args) {
    (void)args;

    console_print("Commands:\n");
    console_print("  help     - list commands\n");
    console_print("  clear    - clear screen\n");
    console_print("  echo     - print text\n");
    console_print("  version  - show kernel version\n");
    console_print("  about    - show OS info\n");
    console_print("  meminfo  - memory status\n");
}

static void cmd_clear(const char* args) {
    (void)args;

    fb_clear(0x00202020);
    console_reset();
}

static void cmd_echo(const char* args) {
    if (args) {
        console_print(args);
    }
    console_putc('\n');
}

static void cmd_version(const char* args) {
    (void)args;

    console_print("Obsidia OS kernel v0.4\n");
}

static void cmd_about(const char* args) {
    (void)args;

    console_print("Obsidia OS: custom hobby kernel built from scratch.\n");
}

static void cmd_meminfo(const char* args) {
    (void)args;

    console_print("Total usable memory: ");
    memory_print_dec(memory_get_total_usable() / 1024 / 1024);
    console_print(" MiB\n");

    console_print("Total pages: ");
    memory_print_dec(memory_get_total_pages());
    console_print("\n");

    console_print("Usable pages: ");
    memory_print_dec(memory_get_usable_pages());
    console_print("\n");

    console_print("Free pages: ");
    memory_print_dec(memory_get_free_pages());
    console_print("\n");
}

void cmd_initrd(const char *args) {
    console_print("Initrd command coming soon\n");
}

void cmd_ls(const char *args) {
    console_print("ls not implemented yet\n");
}

void cmd_cat(const char *args) {
    console_print("cat not implemented yet\n");
}

static command_t commands[] = {
    {"help",    "list commands",        cmd_help},
    {"clear",   "clear screen",         cmd_clear},
    {"echo",    "print text",           cmd_echo},
    {"version", "show kernel version",  cmd_version},
    {"about",   "show OS info",         cmd_about},
    {"meminfo", "memory status",        cmd_meminfo},
};

static const int command_count = sizeof(commands) / sizeof(commands[0]);

void shell_execute(const char *input) {
    // skip leading spaces
    while (*input == ' ') input++;

    // find args (first space)
    const char *args = NULL;
    int cmd_len = 0;

    while (input[cmd_len] && input[cmd_len] != ' ') {
        cmd_len++;
    }

    if (input[cmd_len] == ' ') {
        args = &input[cmd_len + 1];
    }

    // iterate commands
    for (int i = 0; i < command_count; i++) {
        const char *name = commands[i].name;

        int j = 0;
        while (name[j] && j < cmd_len) {
            if (input[j] != name[j]) {
                break;
            }
            j++;
        }

        // match if:
        // - full command name matched
        // - lengths are equal (prevents partial matches like "he" matching "help")
        if (name[j] == '\0' && j == cmd_len) {
            commands[i].func(args);
            return;
        }
    }

    console_print("Unknown command: ");
    console_print(input);
    console_putc('\n');
}
