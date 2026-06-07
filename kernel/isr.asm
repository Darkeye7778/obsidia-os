; isr.asm - NASM interrupt service routine stubs for Obsidia OS
; Provides common stub + table of entry points for IDT.

bits 64

extern isr_handler

section .text

; Common stub: all ISRs jump here after pushing vector (and optional error code)
isr_common:
    ; Save all general purpose registers (order must match registers_t in idt.c)
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Pass pointer to the saved state on stack (the registers_t starts at [rsp] after pushes)
    mov rdi, rsp
    call isr_handler

    ; Restore
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; Remove vector + error_code (16 bytes) that the specific stub pushed
    add rsp, 16

    iretq

; Macro for exceptions that push error code
%macro ISR_ERR 1
global isr%1
isr%1:
    push qword %1     ; vector
    ; error code already pushed by CPU
    jmp isr_common
%endmacro

; Macro for exceptions / IRQs that do NOT push error code (we push dummy 0)
%macro ISR_NOERR 1
global isr%1
isr%1:
    push qword 0      ; dummy error
    push qword %1     ; vector
    jmp isr_common
%endmacro

; CPU exceptions 0-31
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; IRQs 0-15 -> vectors 32-47 (after PIC remap)
ISR_NOERR 32   ; timer
ISR_NOERR 33   ; keyboard
ISR_NOERR 34
ISR_NOERR 35
ISR_NOERR 36
ISR_NOERR 37
ISR_NOERR 38
ISR_NOERR 39
ISR_NOERR 40
ISR_NOERR 41
ISR_NOERR 42
ISR_NOERR 43
ISR_NOERR 44
ISR_NOERR 45
ISR_NOERR 46
ISR_NOERR 47

; Syscall placeholder (vector 0x80 = 128). We can override the gate later for DPL=3.
ISR_NOERR 128

; Table of entry points (used by idt.c to fill IDT)
section .data
global isr_stub_table
isr_stub_table:
    dq isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
    dq isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
    dq isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
    dq isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    dq isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39
    dq isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47
    ; pad up to 128 for now (others can be added)
    times (128 - 48) dq isr128   ; default to syscall stub for higher for simplicity in table
    ; (real code only installs up to 47 + 128 specially)
