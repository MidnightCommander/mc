; x86 Assembly sample file for syntax highlighting
; Exercises all TS captures from asm-highlights.scm

section .data
    msg db "Hello, World!", 0x0A, 0
    len equ $ - msg
    fmt db "Value: %d", 10, 0
    align 16
    buffer times 256 db 0

section .bss
    result resd 1
    input resb 64

section .text
    global _start
    extern printf

; Simple function with label and instructions
_start:
    ; Set up stack frame
    push ebp
    mov ebp, esp
    sub esp, 16

    ; Load effective address
    lea eax, [msg]
    push eax
    call print_string
    add esp, 4

    ; Arithmetic operations
    mov eax, 42
    add eax, 8
    sub eax, 3
    imul eax, 2
    xor edx, edx
    div ecx

    ; Compare and branch
    cmp eax, 100
    jge .large_value
    jl .small_value

.large_value:
    mov dword [result], 1
    jmp .done

.small_value:
    mov dword [result], 0

.done:
    ; String operations
    lea esi, [msg]
    lea edi, [buffer]
    mov ecx, len
    rep movsb

    ; Stack cleanup and exit
    mov esp, ebp
    pop ebp

    ; System call: exit
    mov eax, 1
    xor ebx, ebx
    int 0x80

; Function: print_string
; Input: pointer on stack
print_string:
    push ebp
    mov ebp, esp
    push eax
    push ebx
    push ecx
    push edx

    ; sys_write(stdout, msg, len)
    mov eax, 4
    mov ebx, 1
    mov ecx, [ebp + 8]
    mov edx, len
    int 0x80

    pop edx
    pop ecx
    pop ebx
    pop eax
    pop ebp
    ret

; 64-bit function example
global func64
func64:
    push rbp
    mov rbp, rsp

    ; Use 64-bit registers
    mov rax, rdi
    add rax, rsi
    imul rax, rdx

    ; SSE operations
    movaps xmm0, [rdi]
    addps xmm0, xmm1
    mulps xmm0, xmm2

    ; Pointer size directives
    mov byte [rax], 0
    mov word [rax + 2], 0xFFFF
    mov dword [rax + 4], 12345
    mov qword [rax + 8], 0

    pop rbp
    ret

; Macro-style block comment
/* This is a block comment
   spanning multiple lines */

; Data definitions with various sizes
data_section:
    db 0x41, 0x42, 0x43
    dw 0x1234
    dd 3.14
    dq 123456789

; Conditional assembly
%ifdef DEBUG
    mov eax, 0xDEADBEEF
%else
    xor eax, eax
%endif
%define MAX_SIZE 1024
