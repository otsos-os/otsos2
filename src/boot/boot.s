.intel_syntax noprefix

/* Multiboot 1 Header */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map */
.set GFX,      1<<2             
.set FLAGS,    ALIGN | MEMINFO | GFX
.set MAGIC,    0x1BADB002       /* 'magic number' lets bootloader find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum of above, to prove we are multiboot */

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.long 0, 0, 0, 0, 0 
.long 0    
.long 1024 
.long 768  
.long 32   

.section .bss
.align 4096
p4_table:
    .skip 4096
p3_table:
    .skip 4096
p2_table:
    .skip 4096
p2_table_1:
    .skip 4096
p2_table_2:
    .skip 4096
p2_table_3:
    .skip 4096
stack_bottom:
    .skip 65536
stack_top:

/* Save Multiboot Info pointer */
.section .data
.align 8
multiboot_info_ptr:
    .quad 0

.section .rodata
gdt64:
    .quad 0
    .quad (1<<43) | (1<<44) | (1<<47) | (1<<53)
pointer64:
    .word . - gdt64 - 1
    .quad gdt64

.section .text
.code32
.global start
start:
    /* ebx contains multiboot info structure address */
    mov esp, offset stack_top

    push eax
    push ebx /* Save multiboot info on stack just to be safe */

    cmp eax, 0x2BADB002
    jne .Lno_multiboot

    /* Save ebx to a fixed location so we can retrieve it in 64-bit mode easily 
       (or just ensure it survives the transition) */
    mov [multiboot_info_ptr], ebx

    mov eax, offset p3_table
    or eax, 0b11
    mov [p4_table], eax

    mov eax, offset p3_table
    or eax, 0b11
    mov [p4_table], eax

    
    mov eax, offset p2_table
    or eax, 0b11
    mov [p3_table + 0], eax

    mov eax, offset p2_table_1
    or eax, 0b11
    mov [p3_table + 8], eax

    mov eax, offset p2_table_2
    or eax, 0b11
    mov [p3_table + 16], eax

    mov eax, offset p2_table_3
    or eax, 0b11
    mov [p3_table + 24], eax
    mov ecx, 0
.Lmap_p2_table:
    mov eax, 0x200000
    mul ecx
    or eax, 0b10000011
    
    mov [p2_table + ecx * 8], eax

    inc ecx
    cmp ecx, 2048
    jne .Lmap_p2_table

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, offset p4_table
    mov cr3, eax

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [pointer64]

    push 0x08
    push offset start64
    retf

.Lno_multiboot:
    mov dword ptr [0xb8000], 0x4f524f45
    mov dword ptr [0xb8004], 0x4f3a4f52
    hlt

.code64
start64:
    mov ax, 0x00
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    /* Pass multiboot ptr as first argument (rdi) */
    mov rdi, [multiboot_info_ptr]

    .extern kmain
    call kmain

    hlt
