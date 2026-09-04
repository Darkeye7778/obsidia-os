#pragma once
#include <stdint.h>
#include "idt.h"

#define MAX_TASKS 16
#define TASK_NAME_LEN 32
#define MAX_PROCESSES 16
#define MAX_FDS 16
#define MAX_HANDLES 32
struct kobject;
typedef struct { struct kobject* object; uint32_t generation; uint32_t rights; uint8_t inheritable; } handle_entry_t;

typedef struct { void* object; uint64_t offset; uint8_t used; } fd_entry_t;
typedef struct { uint64_t pid; int64_t status; uint8_t valid; } completion_t;
typedef struct vm_region {
    uint64_t start, length, flags;
    uint8_t kind;
    struct kobject* object;
    struct vm_region* next;
} vm_region_t;

typedef struct process {
    uint64_t pid;
    uint64_t parent_pid;
    char name[TASK_NAME_LEN];
    uint64_t cr3;
    int64_t exit_status;
    uint8_t faulted;
    uint8_t live_threads;
    fd_entry_t fds[MAX_FDS];
    completion_t completions[8];
    vm_region_t* vm_regions;
    handle_entry_t handles[MAX_HANDLES];
} process_t;

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task {
    uint64_t tid;
    process_t* process;
    char name[TASK_NAME_LEN];
    task_state_t state;
    int ring;               // 0 = kernel, 3 = user

    // Context
    uint64_t rip;
    uint64_t rsp;

    // Stacks (physical or virtual pointers)
    uint64_t kstack_base;   // kernel stack base (for rsp0 in TSS on user->kernel)
    uint64_t kstack_size;
    uint64_t ustack_base;   // user stack base (for user tasks)
    uint64_t ustack_size;

    uint64_t entry_point;
    uint8_t fpu_state[512] __attribute__((aligned(16)));
    uint8_t fpu_valid;
    uint8_t owns_kstack;
    uint64_t wake_tick;
    uint8_t wait_reason;
    void* wait_channel;

    // For simple list
    struct task* next;
} task_t;

extern task_t* current_task;
extern task_t* task_list_head;

void tasking_init(void);

// Create a kernel thread (runs in ring 0, cooperative)
task_t* task_create_kernel_thread(void (*entry)(void), const char* name);

// Create a user task (ring 3) - entry must be user virtual, ustack prepared
// Yield to next ready task (cooperative)
void task_yield(void);

// Simple scheduler: pick next ready task
void schedule(void);
registers_t* task_schedule_from_interrupt(registers_t* regs);
void task_request_reschedule(void);
int task_reschedule_requested(void);
void task_terminate_current(int64_t status, int faulted);
void task_block_current(uint8_t reason, uint64_t wake_tick);
void task_wake_input_waiters(void);
void task_block_on(void* channel);
void task_wake_channel(void* channel, int wake_all);
int task_wait_child(uint64_t pid, int64_t* status);

// Get current pid etc for syscalls
uint64_t task_get_current_pid(void);

// For debug
void task_print_list(void);

// Internal: context switch (implemented in asm usually)
void context_switch(task_t* prev, task_t* next);

// User program launch (loader + ring 3 entry). Returns when the program does SYS_EXIT.
int task_run_user_program(const char* filename);
int64_t process_spawn(const char* filename, uint64_t parent_pid);
