#include "shell.h"
#include "../console/console.h"
#include "../drivers/framebuffer.h"
#include "../memory/memory.h"
#include <stddef.h>
#include "../initrd/initrd.h"
#include "../vfs/vfs.h"
#include "../task.h"
#include "../block.h"
#include "../display.h"

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
    console_print("  initrd   - show initrd info\n");
    console_print("  ls       - list root files (VFS)\n");
    console_print("  cat      - read file via VFS\n");
    console_print("  vfsinfo  - VFS mount status\n");
    console_print("  status   - system status (Obsidia style)\n");
    console_print("  demo     - exercise base user/syscall path for GUI\n");
    console_print("  tasks    - list tasks (cooperative multitasking)\n");
    console_print("  run <p>  - load and run user program from VFS (ring 3)\n");
    console_print("  blkdevs  - list block devices\n");
    console_print("  mounts   - list mounted filesystems\n");
    console_print("  readblk <dev> <lba> - read block (debug)\n");
    console_print("  tmpfs_test - test ramfs write/read (Phase 2)\n");
    console_print("  dispinfo   - display abstraction info\n");
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
    (void)args;
    initrd_print_info();
}

void cmd_ls(const char *args) {
    (void)args;
    vfs_node_t* root = vfs_get_root();
    if (!root) {
        console_print("VFS not mounted\n");
        return;
    }
    vfs_list(root);
}

static void cmd_cat(const char* args) {
    if (!args || args[0] == '\0') {
        console_print("Usage: cat <file>\n");
        return;
    }

    vfs_node_t* node = vfs_open(args);
    if (!node) {
        console_print("File not found.\n");
        return;
    }
    if (node->type != VFS_FILE) {
        console_print("Not a file.\n");
        return;
    }

    // Stream read and print (small files)
    uint8_t buf[64];
    uint64_t off = 0;
    while (1) {
        int64_t got = vfs_read(node, off, buf, sizeof(buf));
        if (got <= 0) break;
        for (int64_t i = 0; i < got; i++) {
            console_putc((char)buf[i]);
        }
        off += got;
        if ((uint64_t)got < sizeof(buf)) break;
    }
    console_print("\n");
}

static void cmd_vfsinfo(const char* args) {
    (void)args;
    vfs_node_t* r = vfs_get_root();
    if (r) {
        console_print("VFS root mounted.\n");
        console_print("Root children available via ls.\n");
    } else {
        console_print("VFS not mounted.\n");
    }
}

static void cmd_status(const char* args) {
    (void)args;
    console_print("Obsidia OS base: VFS+IRQ+Timer+Paging+Syscalls ready.\n");
    console_print("No bloat. Custom OAR. Longevity first.\n");
    console_print("Type 'demo' to see GUI handoff path.\n");
}

static void cmd_demo(const char* args) {
    (void)args;
    console_print("=== Obsidia GUI Base Demo ===\n");
    console_print("Syscall numbers defined for user programs (write, yield, fbinfo, etc).\n");
    console_print("VFS mounted for assets/fonts.\n");
    console_print("Input is IRQ driven, timer running.\n");
    console_print("Paging active (user virtual ready).\n");
    console_print("Next: your full GUI can use these foundations.\n");
    // In future: run_user_demo() from syscall when ring3 + GDT complete.
}

static void cmd_tasks(const char* args) {
    (void)args;
    task_print_list();
}

static void cmd_run(const char* args) {
    if (!args || args[0] == '\0') {
        console_print("Usage: run <program>\n");
        return;
    }
    // args may have trailing, take first word
    char prog[64];
    int i=0;
    while (args[i] && args[i] != ' ' && i < 63) {
        prog[i] = args[i];
        i++;
    }
    prog[i] = 0;
    task_run_user_program(prog);
}

static void cmd_blkdevs(const char* args) {
    (void)args;
    block_list();
}

static void cmd_mounts(const char* args) {
    (void)args;
    extern void vfs_list_mounts(void);
    vfs_list_mounts();
}

static void cmd_readblk(const char* args) {
    if (!args) { console_print("Usage: readblk <dev> <lba>\n"); return; }
    // very simple parse first token dev, second lba
    char devname[32]; uint64_t lba = 0;
    int i=0; while (args[i] && args[i]!=' ' && i<31) { devname[i]=args[i]; i++; } devname[i]=0;
    while (args[i]==' ') i++;
    // parse lba decimal simple
    while (args[i]>='0' && args[i]<='9') { lba = lba*10 + (args[i]-'0'); i++; }
    block_device_t* d = block_find(devname);
    if (!d) { console_print("device not found\n"); return; }
    uint8_t buf[512];
    if (block_read(d, lba, 1, buf) == 0) {
        console_print("Block read OK (first bytes): ");
        for (int k=0; k<8; k++) { /* hex omit simple */ console_putc('?'); }
        console_print("\n");
    } else {
        console_print("Block read failed\n");
    }
}

static void cmd_tmpfs_test(const char* args) {
    (void)args;
    vfs_node_t* root = vfs_get_root();
    if (!root) { console_print("no root\n"); return; }
    vfs_node_t* tmpdir = vfs_find_child(root, "tmp");
    if (!tmpdir) { console_print("/tmp not found (ramfs not mounted?)\n"); return; }
    vfs_node_t* f = vfs_create(tmpdir, "test.txt", VFS_FILE);
    if (!f) { console_print("create failed\n"); return; }
    const char* data = "Obsidia ramfs write test OK\n";
    int64_t w = vfs_write(f, 0, data, 28);
    console_print("tmpfs write: "); memory_print_dec(w); console_print(" bytes\n");
    // read back
    char rbuf[32]; int64_t r = vfs_read(f, 0, rbuf, 28);
    if (r > 0) { rbuf[r]=0; console_print("read back: "); console_print(rbuf); }
    console_print("tmpfs test done (initrd remains read-only)\n");
}

static void cmd_dispinfo(const char* args) {
    (void)args;
    display_info_t info;
    display_get_info(&info);
    console_print("Display: w=");
    memory_print_dec(info.width);
    console_print(" h=");
    memory_print_dec(info.height);
    console_print(" pitch=");
    memory_print_dec(info.pitch);
    console_print("\n");
    if (info.has_back) console_print(" (has back buffer)\n");
}

static command_t commands[] = {
    {"help",    "list commands",        cmd_help},
    {"clear",   "clear screen",         cmd_clear},
    {"echo",    "print text",           cmd_echo},
    {"version", "show kernel version",  cmd_version},
    {"about",   "show OS info",         cmd_about},
    {"meminfo", "memory status",        cmd_meminfo},
    {"initrd", "show initrd info",      cmd_initrd},
    {"ls", "list root files (VFS)",     cmd_ls},
    {"cat", "read file via VFS",        cmd_cat},
    {"vfsinfo", "VFS mount status",     cmd_vfsinfo},
    {"status",  "system status",        cmd_status},
    {"demo",    "GUI base demo",        cmd_demo},
    {"tasks",   "list tasks",           cmd_tasks},
    {"run",     "run user program",     cmd_run},
    {"blkdevs", "list block devices",   cmd_blkdevs},
    {"mounts",  "list mounts",          cmd_mounts},
    {"readblk", "read block (debug)",   cmd_readblk},
    {"tmpfs_test", "test ramfs write",  cmd_tmpfs_test},
    {"dispinfo", "display info (Phase 2)", cmd_dispinfo},
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
