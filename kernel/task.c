#include "task.h"
#include "gdt.h"
#include "console/console.h"
#include "memory/heap.h"
#include "memory/memory.h"
#include "paging.h"
#include "vfs/vfs.h"
#include <stddef.h>
#include <stdint.h>

task_t* current_task = 0;
task_t* task_list_head = 0;
static uint64_t next_pid = 1;

static task_t tasks[MAX_TASKS];
static int task_count = 0;

void tasking_init(void) {
    task_list_head = 0;
    current_task = 0;
    task_count = 0;
    for (int i=0; i<MAX_TASKS; i++) {
        tasks[i].pid = 0;
        tasks[i].state = TASK_ZOMBIE;
    }
    console_print("Tasking initialized\n");
}

static task_t* alloc_task_slot(void) {
    for (int i=0; i<MAX_TASKS; i++) {
        if (tasks[i].state == TASK_ZOMBIE || tasks[i].pid == 0) {
            return &tasks[i];
        }
    }
    return 0;
}

static void add_to_list(task_t* t) {
    t->next = 0;
    if (!task_list_head) {
        task_list_head = t;
    } else {
        task_t* cur = task_list_head;
        while (cur->next) cur = cur->next;
        cur->next = t;
    }
}

task_t* task_create_kernel_thread(void (*entry)(void), const char* name) {
    task_t* t = alloc_task_slot();
    if (!t) {
        console_print("TASK: no free task slot\n");
        return 0;
    }

    t->pid = next_pid++;
    t->state = TASK_READY;
    t->ring = 0;
    t->cr3 = paging_get_cr3();
    t->entry_point = (uint64_t)entry;

    // Name
    int i=0;
    for (; name && name[i] && i < TASK_NAME_LEN-1; i++) t->name[i] = name[i];
    t->name[i] = 0;

    // Allocate kernel stack directly from PMM (needs contiguous low pages for the stack range)
    t->kstack_size = 4096 * 2; // 8KiB
    t->kstack_base = (uint64_t)pmm_alloc_pages(2);
    if (!t->kstack_base) {
        console_print("TASK: failed to alloc kstack\n");
        console_print("  free_pages=");
        memory_print_dec(memory_get_free_pages());
        console_print("\n");
        t->state = TASK_ZOMBIE;
        return 0;
    }

    uint64_t stack_top = t->kstack_base + t->kstack_size;

    // Prepare initial stack for kernel thread:
    // We will push a fake return address (the entry) and initial regs if needed.
    // For simplicity, the context switch will expect the stack to have the regs + return rip = entry.
    // Setup minimal: put entry as if "ret" target.
    uint64_t* s = (uint64_t*)stack_top;
    *--s = (uint64_t)entry;   // rip that "ret" will go to after switch pops
    // push some callee saved zeros for initial frame (rbp etc will be popped in switch)
    *--s = 0; // rbp
    *--s = 0; // rbx
    *--s = 0; // r12
    *--s = 0; // r13
    *--s = 0; // r14
    *--s = 0; // r15

    t->rsp = (uint64_t)s;
    t->rip = (uint64_t)entry;  // informational

    t->ustack_base = 0;
    t->ustack_size = 0;

    add_to_list(t);

    if (!current_task) {
        current_task = t;
        t->state = TASK_RUNNING;
    }

    console_print("TASK: created kernel thread '");
    console_print(t->name);
    console_print("'\n");
    return t;
}

task_t* task_create_user_thread(uint64_t entry_point, uint64_t ustack_top, const char* name) {
    task_t* t = alloc_task_slot();
    if (!t) return 0;

    t->pid = next_pid++;
    t->state = TASK_READY;
    t->ring = 3;
    t->cr3 = paging_get_cr3();
    t->entry_point = entry_point;

    int i=0;
    for (; name && name[i] && i < TASK_NAME_LEN-1; i++) t->name[i] = name[i];
    t->name[i] = 0;

    // Kernel stack for when this task traps into kernel (syscalls, IRQs)
    t->kstack_size = 4096 * 2;
    t->kstack_base = (uint64_t)pmm_alloc_pages(2);
    if (!t->kstack_base) {
        console_print("TASK: failed to alloc kstack (user thread)\n");
        console_print("  free_pages=");
        memory_print_dec(memory_get_free_pages());
        console_print("\n");
        t->state = TASK_ZOMBIE;
        return 0;
    }

    // ustack_top is the top (high address) of the user stack already allocated/mapped by loader
    t->ustack_base = 0; // loader knows
    t->ustack_size = 0;

    // Prepare kernel stack? For first switch into user task, the switch will be special (iret from kernel).
    // For cooperative yield from user, the user will have called into syscall YIELD, which will have saved on kstack.
    // For initial creation of user task, we don't run it via normal context_switch yet; the 'run' command will iret directly.
    // Store the user rsp for later.
    t->rsp = ustack_top;  // user rsp initially
    t->rip = entry_point;

    add_to_list(t);

    console_print("TASK: created user thread '");
    console_print(t->name);
    console_print("'\n");
    return t;
}

void task_yield(void) {
    if (!current_task) return;
    schedule();
}

void schedule(void) {
    if (!current_task || !task_list_head) return;

    task_t* prev = current_task;
    task_t* next = prev->next ? prev->next : task_list_head;

    // Find next READY task (skip zombies etc)
    int guard = 0;
    while (next && (next->state != TASK_READY && next != prev) && guard < MAX_TASKS) {
        next = next->next ? next->next : task_list_head;
        guard++;
    }

    if (!next || next == prev || next->state != TASK_READY) {
        // nothing else, stay
        return;
    }

    prev->state = TASK_READY;
    next->state = TASK_RUNNING;
    current_task = next;

    // Update TSS rsp0 so if we are in user and switch, kernel has correct stack for this task
    if (next->ring == 3 && next->kstack_base) {
        tss_set_kernel_stack(next->kstack_base + next->kstack_size);
    }

    context_switch(prev, next);
}

// context_switch is provided by kernel/context.asm (real save/restore + rsp switch)


uint64_t task_get_current_pid(void) {
    return current_task ? current_task->pid : 0;
}

void task_print_list(void) {
    console_print("Tasks:\n");
    task_t* t = task_list_head;
    while (t) {
        console_print("  PID=");
        // simple decimal print (reuse memory or basic)
        char buf[32];
        int pos = 0;
        uint64_t p = t->pid;
        if (p == 0) buf[pos++] = '0';
        while (p > 0 && pos < 30) {
            buf[pos++] = '0' + (p % 10);
            p /= 10;
        }
        while (pos > 0) console_putc(buf[--pos]);
        console_print(" ");
        console_print(t->name);
        console_print(" state=");
        switch (t->state) {
            case TASK_RUNNING: console_print("RUN"); break;
            case TASK_READY: console_print("RDY"); break;
            case TASK_BLOCKED: console_print("BLK"); break;
            default: console_print("ZOM"); break;
        }
        console_print(" ring=");
        console_putc('0' + t->ring);
        console_print("\n");
        t = t->next;
    }
}

// Globals for unwind from EXIT syscall back to shell 'run' caller (must be before use)
static uint64_t saved_launch_rsp = 0;
static void* saved_launch_ip = 0;

// ===== Simple userland loader + ring3 launch for Phase 1 =====

#define USER_CODE_VA   0x400000ULL
#define USER_STACK_VA  0x300000ULL
#define USER_STACK_PAGES 4

static int map_and_copy_user_binary(vfs_node_t* node, uint64_t load_va) {
    if (!node || node->size == 0) return 0;

    uint8_t* temp = (uint8_t*)kmalloc(node->size + 4096);
    if (!temp) return 0;

    int64_t got = vfs_read(node, 0, temp, node->size);
    if (got < (int64_t)node->size) {
        // kfree not critical
        return 0;
    }

    uint64_t remaining = node->size;
    uint64_t offset = 0;
    while (remaining > 0) {
        uint64_t page_va = load_va + offset;
        void* phys = pmm_alloc_page();
        if (!phys) return 0;

        // map with user, writable, present (single AS for phase 1)
        uint64_t flags = (1ULL<<0) /*present*/ | (1ULL<<1) /*writable*/ | (1ULL<<2) /*user*/ ;
        if (!paging_map_page(page_va, (uint64_t)phys, flags)) {
            return 0;
        }

        uint64_t to_copy = (remaining > 4096) ? 4096 : remaining;
        uint8_t* dst = (uint8_t*)page_va;  // identity + mapped
        for (uint64_t c=0; c<to_copy; c++) {
            dst[c] = temp[offset + c];
        }

        offset += 4096;
        remaining -= to_copy;
    }
    // kfree(temp);
    return 1;
}

static uint64_t setup_user_stack(uint64_t stack_va_base) {
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t va = stack_va_base + (uint64_t)i * 4096;
        void* phys = pmm_alloc_page();
        if (!phys) return 0;
        uint64_t flags = (1ULL<<0) | (1ULL<<1) | (1ULL<<2);
        if (!paging_map_page(va, (uint64_t)phys, flags)) return 0;
    }
    return stack_va_base + (uint64_t)USER_STACK_PAGES * 4096; // top
}

int task_run_user_program(const char* filename) {
    vfs_node_t* node = vfs_open(filename);
    if (!node) {
        console_print("run: file not found in VFS\n");
        return -1;
    }
    if (node->type != VFS_FILE) {
        console_print("run: not a file\n");
        return -1;
    }

    // map code
    if (!map_and_copy_user_binary(node, USER_CODE_VA)) {
        console_print("run: failed to map binary\n");
        return -1;
    }

    // setup stack
    uint64_t ustack_top = setup_user_stack(USER_STACK_VA);
    if (!ustack_top) {
        console_print("run: failed to setup user stack\n");
        return -1;
    }

    // create the task struct (for 'tasks' listing and future scheduler)
    task_t* ut = task_create_user_thread(USER_CODE_VA, ustack_top, filename);
    if (!ut) {
        console_print("run: failed to create user task\n");
        return -1;
    }

    console_print("Loaded user program '");
    console_print(filename);
    console_print("' at 0x400000, stack @ 0x300000. Launching in ring 3...\n");

    // Prepare TSS kernel stack for when this user does int 0x80 / IRQ
    if (ut->kstack_base) {
        tss_set_kernel_stack(ut->kstack_base + ut->kstack_size);
    }

    uint16_t ucs = gdt_get_user_code_selector();
    uint16_t uds = gdt_get_user_data_selector();
    uint64_t rflags = 0x202; // IF

    // Save unwind point for EXIT syscall
    __asm__ volatile ("mov %%rsp, %0" : "=m"(saved_launch_rsp) : : "memory");
    saved_launch_ip = &&after_user;

    // Push iret frame (reverse order on stack) and enter ring 3
    __asm__ volatile (
        "push %[ss]\n"
        "push %[ursp]\n"
        "push %[rfl]\n"
        "push %[ucs]\n"
        "push %[uip]\n"
        "iretq\n"
        :
        : [ss] "r"((uint64_t)uds),
          [ursp] "r"(ustack_top),
          [rfl] "r"(rflags),
          [ucs] "r"((uint64_t)ucs),
          [uip] "r"(USER_CODE_VA)
        : "memory"
    );

after_user:
    console_print("User program exited.\n");
    return 0;
}

// Helper for syscall EXIT to unwind the ring3 launch
void task_unwind_from_user_exit(void) {
    if (saved_launch_rsp && saved_launch_ip) {
        __asm__ volatile (
            "mov %0, %%rsp\n"
            "jmp *%1\n"
            :
            : "m"(saved_launch_rsp), "m"(saved_launch_ip)
            : "memory"
        );
    }
    for(;;) __asm__ volatile("hlt");
}

