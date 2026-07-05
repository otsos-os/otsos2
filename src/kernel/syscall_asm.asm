

[BITS 64]

section .text

extern syscall_handler
extern smp_lock
extern smp_unlock
extern smp_tss_current

%define USER_DS 0x1B
%define USER_CS 0x23

section .data
syscall_entry_lock:
    dq 0
syscall_user_rsp:
    dq 0

align 16
syscall_bootstrap_stack:
    times 4096 db 0
syscall_bootstrap_stack_top:

global syscall_entry
syscall_entry:
    cli
.lock:
    lock bts qword [rel syscall_entry_lock], 0
    jc .lock
    mov [rel syscall_user_rsp], rsp
    lea rsp, [rel syscall_bootstrap_stack_top]
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
    push qword [rel syscall_user_rsp]
    mov r15, rsp
    call smp_lock
    call smp_tss_current
    mov rsp, [rax + 4]
    push qword USER_DS
    push qword [r15 + 0]
    push qword [r15 + 40]
    push qword USER_CS
    push qword [r15 + 104]
    push qword 0
    push qword 0
    push qword [r15 + 120]
    push qword [r15 + 112]
    push qword [r15 + 104]
    push qword [r15 + 96]           ; rdx
    push qword [r15 + 88]           ; rsi
    push qword [r15 + 80]           ; rdi
    push qword [r15 + 72]           ; rbp
    push qword [r15 + 64]           ; r8
    push qword [r15 + 56]           ; r9
    push qword [r15 + 48]           ; r10
    push qword [r15 + 40]           ; r11
    push qword [r15 + 32]           ; r12
    push qword [r15 + 24]           ; r13
    push qword [r15 + 16]           ; r14
    push qword [r15 + 8]            ; r15
    lock btr qword [rel syscall_entry_lock], 0
    mov rdi, rsp
    call syscall_handler
    call smp_unlock

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
    o64 sysret
