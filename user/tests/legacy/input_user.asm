bits 64
global _start
_start:
    mov rax, 6
    mov rdi, 1
    lea rsi, [rel prompt]
    mov rdx, prompt_len
    int 0x80
    mov rax, 5
    xor rdi, rdi
    lea rsi, [rel input_byte]
    mov rdx, 1
    int 0x80
    mov rax, 6
    mov rdi, 1
    lea rsi, [rel input_byte]
    mov rdx, 1
    int 0x80
    xor rax, rax
    xor rdi, rdi
    int 0x80
prompt: db "input> "
prompt_len equ $-prompt
input_byte: db 0
