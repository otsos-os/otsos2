

[bits 64]

; Segment selectors for Ring 3
%define USER_DS 0x1B    ; GDT_USER_DATA | 3
%define USER_CS 0x23    ; GDT_USER_CODE | 3

section .text

; void userspace_enter(u64 entry, u64 user_stack, u64 user_cs, u64 user_ds)
; rdi = entry point
; rsi = user stack pointer
; rdx = user CS (should be 0x1B)
; rcx = user DS (should be 0x23)
;
; This function performs an iretq to jump to Ring 3
; Stack layout for iretq:
;   [rsp+0]  = RIP (entry point)
;   [rsp+8]  = CS (user code segment)
;   [rsp+16] = RFLAGS
;   [rsp+24] = RSP (user stack)
;   [rsp+32] = SS (user data segment)
global userspace_enter
userspace_enter:
    ; Save arguments before we modify stack
    mov rax, rdi        ; entry point
    mov rbx, rsi        ; user stack
    mov r10, rdx        ; user CS
    mov r11, rcx        ; user DS
    
    ; Set up data segments for user mode
    ; SS will be set by iretq, but we set DS/ES/FS/GS here
    mov cx, USER_DS
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx
    
    ; Build iretq frame on stack
    push r11            ; SS (user data segment)
    push rbx            ; RSP (user stack)
    pushfq              ; RFLAGS (current, will be modified)
    
    ; Enable interrupts in RFLAGS for user mode
    or qword [rsp], 0x200
    
    push r10            ; CS (user code segment)
    push rax            ; RIP (entry point)
    
    ; Clear general purpose registers (security)
    xor rax, rax
    xor rbx, rbx
    xor rcx, rcx
    xor rdx, rdx
    xor rsi, rsi
    xor rdi, rdi
    xor rbp, rbp
    xor r8, r8
    xor r9, r9
    xor r10, r10
    xor r11, r11
    xor r12, r12
    xor r13, r13
    xor r14, r14
    xor r15, r15
    
    ; Jump to userspace!
    iretq
