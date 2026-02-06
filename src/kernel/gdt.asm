
[bits 64]

; GDT segment selectors
%define KERNEL_CS 0x08
%define KERNEL_DS 0x10

section .text

; void gdt_flush(u64 gdt_ptr_addr)
; Load new GDT and reload segment registers
global gdt_flush
gdt_flush:
    lgdt [rdi]              ; Load GDT from pointer
    
    ; Reload CS using far return
    push KERNEL_CS          ; Push new CS
    lea rax, [rel .reload]  ; Get address of next instruction
    push rax                ; Push return address
    retfq                   ; Far return to reload CS
    
.reload:
    ; Reload data segment registers
    mov ax, KERNEL_DS
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ret

; void tss_load(u16 selector)
; Load Task Register with TSS selector
global tss_load
tss_load:
    ltr di                  ; Load Task Register (16-bit selector in di)
    ret
