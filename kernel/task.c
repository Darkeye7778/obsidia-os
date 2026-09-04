#include "task.h"
#include "gdt.h"
#include "console/console.h"
#include "memory/heap.h"
#include "memory/memory.h"
#include "paging.h"
#include "idt.h"
#include "vfs/vfs.h"
#include "timer.h"
#include "elf.h"
#include "object.h"
#include "resource.h"
#include <stddef.h>
#include <stdint.h>

extern void serial_write(const char* str);

task_t* current_task = 0;
task_t* task_list_head = 0;
static uint64_t next_pid = 1;
static uint64_t next_tid = 1;

static task_t tasks[MAX_TASKS];
static process_t processes[MAX_PROCESSES];
static process_t* kernel_process;
static int task_count = 0;
static volatile int reschedule_requested = 0;
static int user_preemption_reported = 0;
static uint8_t initial_fpu_state[512] __attribute__((aligned(16)));

static void fpu_copy(uint8_t* d,const uint8_t* s){for(int i=0;i<512;i++)d[i]=s[i];}
static void fpu_init_task(task_t* t){fpu_copy(t->fpu_state,initial_fpu_state);t->fpu_valid=1;}

static void serial_dec(uint64_t n) {
    char b[21]; int i=20; b[i]=0;
    if(!n){serial_write("0");return;}
    while(n && i){b[--i]=(char)('0'+n%10);n/=10;}
    serial_write(&b[i]);
}

#define USER_CODE_VA   0x400000ULL
#define USER_STACK_VA  0x300000ULL
#define USER_STACK_PAGES 4

static void clear_task(task_t* t) {
    for (uint64_t i = 0; i < sizeof(*t); i++) ((uint8_t*)t)[i] = 0;
    t->state = TASK_ZOMBIE;
}

static process_t* alloc_process(void) {
    for(int i=0;i<MAX_PROCESSES;i++) if(!processes[i].pid) {
        for(uint64_t j=0;j<sizeof(processes[i]);j++) ((uint8_t*)&processes[i])[j]=0;
        processes[i].pid=next_pid++;
        processes[i].fds[0].used=processes[i].fds[1].used=processes[i].fds[2].used=1;
        return &processes[i];
    }
    return 0;
}

static void clear_process(process_t* p) {
    for(uint64_t j=0;j<sizeof(*p);j++) ((uint8_t*)p)[j]=0;
}

void tasking_init(void) {
    task_list_head = 0;
    current_task = 0;
    task_count = 0;
    for(int i=0;i<MAX_PROCESSES;i++) clear_process(&processes[i]);
    uint64_t cr0,cr4;
    __asm__ volatile("mov %%cr0,%0":"=r"(cr0)); cr0&=~(1ULL<<2); cr0|=(1ULL<<1);
    __asm__ volatile("mov %0,%%cr0"::"r"(cr0):"memory");
    __asm__ volatile("mov %%cr4,%0":"=r"(cr4)); cr4|=(1ULL<<9)|(1ULL<<10);
    __asm__ volatile("mov %0,%%cr4"::"r"(cr4):"memory");
    __asm__ volatile("fninit; fxsave64 %0":"=m"(initial_fpu_state));
    for (int i=0; i<MAX_TASKS; i++) {
        clear_task(&tasks[i]);
    }
    task_t* boot = &tasks[0];
    kernel_process=alloc_process();
    kernel_process->cr3=paging_get_kernel_cr3(); kernel_process->live_threads=1;
    boot->tid = next_tid++;
    boot->process=kernel_process;
    boot->state = TASK_RUNNING;
    boot->ring = 0;
    boot->owns_kstack = 0;
    fpu_init_task(boot);
    const char* n = "kernel-main";
    int j=0; while (n[j] && j<TASK_NAME_LEN-1) { boot->name[j]=n[j]; kernel_process->name[j]=n[j]; j++; }
    boot->name[j]=0;
    task_list_head = current_task = boot;
    console_print("Tasking initialized\n");
}

static task_t* alloc_task_slot(void) {
    for (int i=0; i<MAX_TASKS; i++) {
        // Zombie slots remain linked until the process reaper removes them.
        // Reusing one here would splice the same object into the list twice.
        if (tasks[i].tid == 0) {
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

    t->tid = next_tid++;
    t->process=kernel_process; kernel_process->live_threads++;
    t->state = TASK_READY;
    t->ring = 0;
    t->entry_point = (uint64_t)entry;
    t->owns_kstack = 1;
    fpu_init_task(t);

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
        clear_task(t);
        return 0;
    }

    uint64_t stack_top = t->kstack_base + t->kstack_size;

    // A ring-0 IRET frame contains RIP, CS and RFLAGS (no saved RSP/SS).
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    for (uint64_t z=0; z<sizeof(*frame); z++) ((uint8_t*)frame)[z]=0;
    frame->rip = (uint64_t)entry;
    frame->cs = gdt_get_kernel_code_selector();
    frame->rflags = 0x202;
    frame->rsp = stack_top;
    frame->ss = gdt_get_kernel_data_selector();
    t->rsp = (uint64_t)frame;
    t->rip = (uint64_t)entry;  // informational

    t->ustack_base = 0;
    t->ustack_size = 0;

    add_to_list(t);

    console_print("TASK: created kernel thread '");
    console_print(t->name);
    console_print("'\n");
    return t;
}

static task_t* task_create_user_thread_for(process_t* process, uint64_t entry_point, uint64_t ustack_top, const char* name) {
    task_t* t = alloc_task_slot();
    if (!t) return 0;

    t->tid = next_tid++;
    t->process=process; process->live_threads++;
    t->state = TASK_READY;
    t->ring = 3;
    t->entry_point = entry_point;
    t->owns_kstack = 1;
    fpu_init_task(t);

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
        clear_task(t);
        return 0;
    }

    t->ustack_base = USER_STACK_VA;
    t->ustack_size = USER_STACK_PAGES * 4096;

    uint64_t stack_top = t->kstack_base + t->kstack_size;
    registers_t* frame = (registers_t*)(stack_top - sizeof(registers_t));
    for (uint64_t z=0; z<sizeof(*frame); z++) ((uint8_t*)frame)[z]=0;
    frame->rip = entry_point;
    frame->cs = gdt_get_user_code_selector();
    frame->rflags = 0x202;
    frame->rsp = ustack_top;
    frame->ss = gdt_get_user_data_selector();
    t->rsp = (uint64_t)frame;
    t->rip = entry_point;

    add_to_list(t);

    console_print("TASK: created user thread '");
    console_print(t->name);
    console_print("'\n");
    return t;
}

void task_yield(void) {
    __asm__ volatile ("mov $3, %%rax; int $0x80" : : : "rax", "memory");
}

void schedule(void) {
    task_yield();
}

void task_request_reschedule(void) { reschedule_requested = 1; }
int task_reschedule_requested(void) { return reschedule_requested; }

void task_terminate_current(int64_t status, int faulted) {
    if (!current_task) return;
    process_t* dying=current_task->process;
    dying->exit_status = status;
    dying->faulted = faulted ? 1 : 0;
    current_task->state = TASK_ZOMBIE;
    for (task_t* t=task_list_head; t; t=t->next) {
        if (t->process && t->process->pid == dying->parent_pid) {
            for(int i=0;i<8;i++) if(!t->process->completions[i].valid) {
                t->process->completions[i].pid=dying->pid;
                t->process->completions[i].status=faulted ? (-256 + status) : status;
                t->process->completions[i].valid=1; break;
            }
        }
        if (t->state == TASK_BLOCKED && t->wait_reason == 3 && t->wake_tick == dying->pid) {
            t->wait_reason = 0;
            t->state = TASK_READY;
        }
    }
    reschedule_requested = 1;
}

int task_wait_child(uint64_t pid, int64_t* status) {
    if (!current_task || !pid) return -1;
    process_t* p=current_task->process;
    for(int i=0;i<8;i++) if(p->completions[i].valid && p->completions[i].pid==pid) {
        if(status) *status=p->completions[i].status;
        p->completions[i].valid=0; return 1;
    }
    for (task_t* t=task_list_head; t; t=t->next) {
        if (t->process && t->process->pid==pid && t->process->parent_pid==p->pid && t->state!=TASK_ZOMBIE)
            return 0;
    }
    return -1;
}

void task_block_current(uint8_t reason, uint64_t wake_tick) {
    if (!current_task) return;
    current_task->wait_reason = reason;
    current_task->wake_tick = wake_tick;
    current_task->state = TASK_BLOCKED;
    reschedule_requested = 1;
}
void task_block_on(void* channel){if(!current_task||!channel)return;current_task->wait_channel=channel;current_task->state=TASK_BLOCKED;reschedule_requested=1;}
void task_wake_channel(void* channel,int all){for(task_t* t=task_list_head;t;t=t->next)if(t->state==TASK_BLOCKED&&t->wait_channel==channel){t->wait_channel=0;t->state=TASK_READY;if(!all)break;}}

void task_wake_input_waiters(void) {
    for (task_t* t=task_list_head; t; t=t->next) {
        if (t->state == TASK_BLOCKED && t->wait_reason == 1) {
            t->wait_reason = 0;
            t->state = TASK_READY;
        }
    }
}

static void reap_zombies(task_t* executing) {
    task_t* prev = 0;
    task_t* t = task_list_head;
    while (t) {
        task_t* next = t->next;
        if (t != executing && t->state == TASK_ZOMBIE && t->tid) {
            if (prev) prev->next = next; else task_list_head = next;
            process_t* proc=t->process;
            if(proc && proc->live_threads) proc->live_threads--;
            for (int fd=3; proc && fd<MAX_FDS; fd++) if (proc->fds[fd].used) {
                vfs_file_release((open_file_t*)proc->fds[fd].object);
                proc->fds[fd].used=0;
            }
            if (t->ring == 3 && proc && !proc->live_threads && proc->cr3 != paging_get_kernel_cr3())
                paging_destroy_user_address_space(proc->cr3);
            if(proc && !proc->live_threads) { vm_region_t* v=proc->vm_regions; while(v){vm_region_t* n=v->next;if(v->object)object_release(v->object);kfree(v);v=n;} proc->vm_regions=0; }
            if(proc && !proc->live_threads) handles_close_all(proc);
            if (t->owns_kstack && t->kstack_base)
                pmm_free_pages((void*)t->kstack_base, t->kstack_size / 4096);
            clear_task(t);
            if(proc && !proc->live_threads && proc!=kernel_process) clear_process(proc);
            serial_write("TASK: reaped process; free pages=");
            serial_dec(memory_get_free_pages()); serial_write("\n");
        } else {
            prev = t;
        }
        t = next;
    }
}

registers_t* task_schedule_from_interrupt(registers_t* regs) {
    reschedule_requested = 0;
    if (!current_task || !task_list_head) return regs;
    task_t* prev = current_task;
    if(prev->fpu_valid) __asm__ volatile("fxsave64 %0":"=m"(prev->fpu_state));
    uint64_t now = timer_get_ticks();
    for (task_t* w=task_list_head; w; w=w->next) {
        if (w->state == TASK_BLOCKED && w->wait_reason == 2 && now >= w->wake_tick) {
            w->wait_reason = 0;
            w->state = TASK_READY;
        }
    }
    if (!user_preemption_reported && prev->ring == 3 && regs->vector == 32) {
        user_preemption_reported = 1;
        serial_write("SCHED: timer preempted ring-3 process\n");
    }
    prev->rsp = (uint64_t)regs;
    prev->rip = regs->rip;
    if (prev->state == TASK_RUNNING) prev->state = TASK_READY;

    reap_zombies(prev);
    task_t* next = prev->next ? prev->next : task_list_head;
    int guard = 0;
    while (next && next->state != TASK_READY && guard++ < MAX_TASKS) {
        next = next->next ? next->next : task_list_head;
    }
    if (!next || next->state != TASK_READY) {
        if (prev->state != TASK_ZOMBIE) prev->state = TASK_RUNNING;
        return regs;
    }

    next->state = TASK_RUNNING;
    current_task = next;
    if(next->fpu_valid) __asm__ volatile("fxrstor64 %0"::"m"(next->fpu_state));
    if (next->ring == 3)
        tss_set_kernel_stack(next->kstack_base + next->kstack_size);
    paging_activate(next->process && next->process->cr3 ? next->process->cr3 : paging_get_kernel_cr3());
    return (registers_t*)next->rsp;
}

// context_switch is provided by kernel/context.asm (real save/restore + rsp switch)


uint64_t task_get_current_pid(void) {
    return current_task && current_task->process ? current_task->process->pid : 0;
}

void task_print_list(void) {
    console_print("Tasks:\n");
    task_t* t = task_list_head;
    while (t) {
        console_print("  PID=");
        // simple decimal print (reuse memory or basic)
        char buf[32];
        int pos = 0;
        uint64_t p = t->process ? t->process->pid : 0;
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

// ===== Simple userland loader + ring3 launch for Phase 1 =====

static int map_and_copy_user_binary(vfs_node_t* node, uint64_t cr3, uint64_t load_va) {
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
        if (!paging_map_page_in(cr3, page_va, (uint64_t)phys, flags)) {
            return 0;
        }

        uint64_t to_copy = (remaining > 4096) ? 4096 : remaining;
        uint8_t* dst = (uint8_t*)phys;
        for (uint64_t c=0; c<to_copy; c++) {
            dst[c] = temp[offset + c];
        }

        offset += 4096;
        remaining -= to_copy;
    }
    // kfree(temp);
    return 1;
}

static uint64_t setup_user_stack(uint64_t cr3, uint64_t stack_va_base) {
    for (int i = 0; i < USER_STACK_PAGES; i++) {
        uint64_t va = stack_va_base + (uint64_t)i * 4096;
        void* phys = pmm_alloc_page();
        if (!phys) return 0;
        uint64_t flags = (1ULL<<0) | (1ULL<<1) | (1ULL<<2);
        if (!paging_map_page_in(cr3, va, (uint64_t)phys, flags)) return 0;
    }
    return stack_va_base + (uint64_t)USER_STACK_PAGES * 4096; // top
}

int64_t process_spawn(const char* filename, uint64_t parent_pid) {
    vfs_node_t* node = vfs_open(filename);
    if (!node) {
        serial_write("USER: executable not found\n");
        console_print("run: file not found in VFS\n");
        return -1;
    }
    if (node->type != VFS_FILE) {
        console_print("run: not a file\n");
        return -1;
    }

    uint8_t magic[4];
    int is_elf = vfs_read(node,0,magic,sizeof(magic)) == 4 &&
        magic[0]==0x7f && magic[1]=='E' && magic[2]=='L' && magic[3]=='F';
    uint64_t process_cr3=0, entry=USER_CODE_VA, ustack_top=0;
    if (is_elf) {
        if (!elf_load_process(node,&process_cr3,&entry,&ustack_top)) {
            serial_write("USER: invalid or unloadable ELF64 executable\n");
            return -1;
        }
    } else {
        process_cr3 = paging_create_user_address_space();
        if (!process_cr3 || !map_and_copy_user_binary(node,process_cr3,USER_CODE_VA) ||
            !(ustack_top=setup_user_stack(process_cr3,USER_STACK_VA))) {
            serial_write("USER: raw executable mapping failed\n");
            if (process_cr3) paging_destroy_user_address_space(process_cr3);
            return -1;
        }
    }

    // create the task struct (for 'tasks' listing and future scheduler)
    process_t* proc=alloc_process();
    if(!proc) { paging_destroy_user_address_space(process_cr3); return -1; }
    proc->cr3=process_cr3; proc->parent_pid=parent_pid;
    for(int pi=0;pi<MAX_PROCESSES;pi++)if(processes[pi].pid==parent_pid){handles_inherit(proc,&processes[pi]);break;}
    if(kernel_process && parent_pid==kernel_process->pid) resource_grant_input(proc);
    int ni=0; while(filename[ni]&&ni<TASK_NAME_LEN-1){proc->name[ni]=filename[ni];ni++;}
    task_t* ut = task_create_user_thread_for(proc, entry, ustack_top, filename);
    if (!ut) {
        serial_write("USER: task allocation failed\n");
        console_print("run: failed to create user task\n");
        paging_destroy_user_address_space(process_cr3);
        clear_process(proc);
        return -1;
    }

    console_print("Scheduled user program '");
    console_print(filename);
    console_print(is_elf ? "' (ELF64).\n" : "' (raw compatibility image).\n");
    return (int64_t)proc->pid;
}

int task_run_user_program(const char* filename) {
    uint64_t irq_flags;
    __asm__ volatile("pushfq; pop %0; cli" : "=r"(irq_flags) : : "memory");
    uint64_t parent = current_task && current_task->process ? current_task->process->pid : 0;
    int64_t pid = process_spawn(filename,parent);
    if (pid < 0) {
        if (irq_flags & (1ULL<<9)) __asm__ volatile("sti" : : : "memory");
        return -1;
    }
    if (current_task) task_block_current(3,(uint64_t)pid);
    if (irq_flags & (1ULL<<9)) __asm__ volatile("sti" : : : "memory");
    return 0;
}
