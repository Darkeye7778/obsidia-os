#pragma once
#include <stdint.h>
#include "idt.h"

#define MAX_TASKS 16
#define TASK_NAME_LEN 32

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task {
    uint64_t pid;
    char name[TASK_NAME_LEN];
    task_state_t state;
    int ring;               // 0 = kernel, 3 = user

    // Context
    uint64_t rip;
    uint64_t rsp;
    uint64_t cr3;           // page table root (for future per-process; currently shared)

    // Stacks (physical or virtual pointers)
    uint64_t kstack_base;   // kernel stack base (for rsp0 in TSS on user->kernel)
    uint64_t kstack_size;
    uint64_t ustack_base;   // user stack base (for user tasks)
    uint64_t ustack_size;

    uint64_t entry_point;
    uint8_t owns_kstack;
    int64_t exit_status;
    uint8_t faulted;
    uint64_t wake_tick;
    uint8_t wait_reason;
    uint64_t parent_pid;
    struct { uint64_t pid; int64_t status; uint8_t valid; } completions[8];
    struct { void* object; uint64_t offset; uint8_t used; } fds[16];

    // For simple list
    struct task* next;
} task_t;

extern task_t* current_task;
extern task_t* task_list_head;

void tasking_init(void);

// Create a kernel thread (runs in ring 0, cooperative)
task_t* task_create_kernel_thread(void (*entry)(void), const char* name);

// Create a user task (ring 3) - entry must be user virtual, ustack prepared
task_t* task_create_user_thread(uint64_t entry_point, uint64_t ustack_top, const char* name);

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
