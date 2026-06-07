; context.asm - cooperative context switch for Obsidia
; void context_switch(task_t* prev, task_t* next)
; Saves callee-saved registers + switches stack using task->rsp field.

bits 64

global context_switch
extern console_print   ; for debug if wanted

; Offsets into task_t (must match struct layout in task.h)
; pid 0, name 8, state 40, ring 44, rip 48, rsp 56, cr3 64, ...
%define TASK_RSP_OFFSET 56

section .text

context_switch:
    ; rdi = prev, rsi = next

    ; Save callee-saved registers on current stack (the order we pop must reverse)
    push r15
    push r14
    push r13
    push r12
    push rbx
    push rbp

    ; Save the current stack pointer into prev->rsp
    mov [rdi + TASK_RSP_OFFSET], rsp

    ; Load next task's stack
    mov rsp, [rsi + TASK_RSP_OFFSET]

    ; Restore callee-saved for the new task
    pop rbp
    pop rbx
    pop r12
    pop r13
    pop r14
    pop r15

    ; Return into whatever is on the new stack (for new threads this is the entry point we pushed)
    ret
