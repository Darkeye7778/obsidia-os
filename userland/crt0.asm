bits 64
global _start
extern obsidia_main

section .text
_start:
    xor rbp, rbp
    and rsp, -16
    xor edi, edi                    ; argc = 0 (argument vectors follow later)
    xor esi, esi                    ; argv = NULL
    xor edx, edx                    ; envp = NULL
    call obsidia_main               ; callee observes RSP == 8 mod 16
    mov rdi, rax
    xor eax, eax                    ; SYS_EXIT
    int 0x80
    ud2

section .note.GNU-stack noalloc noexec nowrite progbits
