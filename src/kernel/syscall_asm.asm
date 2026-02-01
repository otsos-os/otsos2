[BITS 64]

section .text

extern syscall_handler
extern tss_get_rsp0

%define USER_DS 0x1B
%define USER_CS 0x23

global syscall_entry
syscall_entry:
    ; syscall saves RIP to RCX and RFLAGS to R11
    ; It does NOT switch stack automatically!
    
    ; 1. Save user RSP
    mov [rel user_rsp_save], rsp
    
    ; 2. Switch to kernel stack
    call tss_get_rsp0
    mov rsp, rax
    
    ; 3. Construct registers_t structure on stack
    ; Layout (from top to bottom):
    ; ss, rsp, rflags, cs, rip, err_code, int_no, rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15
    
    push qword USER_DS      ; ss
    push qword [rel user_rsp_save] ; rsp
    push r11                ; rflags
    push qword USER_CS      ; cs
    push rcx                ; rip
    push qword 0            ; err_code
    push qword 0            ; int_no
    
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
    
    ; 4. Call C handler
    mov rdi, rsp ; Pass pointer to registers_t
    call syscall_handler
    
    ; 5. Restore registers
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
    
    ; Skip int_no and err_code
    add rsp, 16
    
    ; 6. Prepare for sysretq
    pop rcx ; RIP
    add rsp, 8 ; Skip CS
    pop r11 ; RFLAGS
    pop rsp ; User RSP
    ; SS is implicitly set by sysretq based on STAR MSR
    
    sysretq

section .bss
user_rsp_save:
    resq 1
