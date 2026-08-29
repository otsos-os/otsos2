

[BITS 64]

section .text

extern syscall_handler
extern smp_lock
extern smp_unlock

%define USER_DS 0x1B
%define USER_CS 0x23

%define PCPU_SYSCALL_SCRATCH 8
%define PCPU_SYSCALL_STACK   16

global syscall_entry
syscall_entry:
    swapgs
    mov [gs:PCPU_SYSCALL_SCRATCH], rsp
    mov rsp, [gs:PCPU_SYSCALL_STACK]
    test rsp, rsp
    jz .no_stack

    push qword USER_DS
    push qword [gs:PCPU_SYSCALL_SCRATCH]
    push r11
    push qword USER_CS
    push rcx
    push qword 0
    push qword 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call smp_lock
    mov rdi, rsp
    call syscall_handler
    call smp_unlock

    cli
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    pop rcx
    add rsp, 8
    pop r11
    pop rsp
    swapgs
    o64 sysret

.no_stack:
    cli
.hang:
    hlt
    jmp .hang
