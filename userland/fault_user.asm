bits 64
global _start
_start:
    mov rax, [0x70000000]       ; deliberately unmapped user page
    jmp $
