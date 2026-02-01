[BITS 64]

section .text

extern syscall_handler
extern tss 

%define USER_DS 0x1B
%define USER_CS 0x23
; Offset of rsp0 in struct tss_t (u32 reserved0; u64 rsp0)
; u32 is 4 bytes. Packed/Aligned behavior needs verification.
; The C define has __attribute__((packed)).
; So offset is 4.
%define TSS_RSP0_OFFSET 4 

global syscall_entry
syscall_entry:
    ; 1. Save User RSP to a temporary location
    ; (Note: In a multi-core/preemptive OS, this should be per-cpu)
    mov [rel user_rsp_save], rsp
    
    ; 2. Switch to Kernel Stack
    ; We read directly from the TSS structure to avoid function calls clobbering regs
    mov rsp, [rel tss + TSS_RSP0_OFFSET]
    
    ; 3. Build interrupt stack frame (registers_t)
    ; Stack grows down. We push in reverse order of the struct.
    
    push qword USER_DS              ; ss
    push qword [rel user_rsp_save]  ; rsp (saved user stack)
    push r11                        ; rflags (saved by syscall)
    push qword USER_CS              ; cs
    push rcx                        ; rip (saved by syscall)
    push qword 0                    ; err_code
    push qword 0                    ; int_no
    
    push rax                        ; syscall_number
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
    
    ; 4. Call Handler
    mov rdi, rsp
    call syscall_handler
    
    ; 5. Restore state
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
    pop rax ; Return value from syscall
    
    add rsp, 16 ; Skip int_no, err_code
    
    ; 6. Return to User Mode
    pop rcx ; RIP
    add rsp, 8 ; Skip CS
    pop r11 ; RFLAGS
    pop rsp ; User RSP
    
    o64 sysret

section .bss
user_rsp_save: resq 1
