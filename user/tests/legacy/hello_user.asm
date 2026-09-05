; hello_user.asm - tiny flat userland program for Obsidia
; nasm -f bin -o userland/hello_user.bin userland/hello_user.asm
bits 64

global _start
_start:
    ; print hello
    mov rax, 1          ; SYS_WRITE
    lea rdi, [rel msg]
    mov rsi, msglen
    int 0x80

    ; Deterministic CPU-bound interval: this contains no yield or syscall and
    ; must be interrupted by the PIT for kernel tasks to keep running.
    mov rcx, 50000000
.preempt_test:
    dec rcx
    jnz .preempt_test

    ; get ticks
    mov rax, 2          ; SYS_GETTICKS
    int 0x80
    ; rax now has ticks, we could print but keep simple (no easy print num yet)

    ; yield a few times
    mov rcx, 3
.yield_loop:
    mov rax, 3          ; SYS_YIELD
    int 0x80
    dec rcx
    jnz .yield_loop

    ; exit cleanly
    mov rax, 0          ; SYS_EXIT
    int 0x80

    ; shouldn't reach
    jmp $

msg: db "Hello from userland", 10, 0
msglen equ $ - msg - 1   ; length without null
