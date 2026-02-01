.intel_syntax noprefix

/* Multiboot2 Header */
.set MB2_MAGIC,        0xE85250D6          /* Multiboot2 magic number */
.set MB2_ARCH,         0                   /* i386 protected mode */
.set MB2_HEADER_LEN,   multiboot2_header_end - multiboot2_header
.set MB2_CHECKSUM,     -(MB2_MAGIC + MB2_ARCH + MB2_HEADER_LEN)

.set MB1_BOOTLOADER_MAGIC, 0x2BADB002
.set MB2_BOOTLOADER_MAGIC, 0x36D76289

.section .multiboot
.align 8
multiboot2_header:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_HEADER_LEN
    .long MB2_CHECKSUM

    .align 8
fb_tag_start:
    .word 5                                 /* type = framebuffer */
    .word 0                                 /* flags = optional (0) */
    .long fb_tag_end - fb_tag_start         /* size */
    .long 1024                              /* width */
    .long 768                               /* height */
    .long 32                                /* depth (bpp) */
fb_tag_end:

    /* End tag */
    .align 8
end_tag:
    .word 0                                 /* type = end */
    .word 0                                 /* flags */
    .long 8                                 /* size */
multiboot2_header_end:

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

.section .data
.align 8
multiboot_info_ptr:
    .quad 0
multiboot_magic_val:
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
    mov esp, offset stack_top

    mov [multiboot_magic_val], eax
    mov [multiboot_info_ptr], ebx

    cmp eax, MB2_BOOTLOADER_MAGIC
    je .Lvalid_multiboot
    cmp eax, MB1_BOOTLOADER_MAGIC
    je .Lvalid_multiboot
    jmp .Lno_multiboot

.Lvalid_multiboot:
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
    or eax, 0b10000011           /* Present + Writable + Huge */
    
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
    mov dword ptr [0xb8000], 0x4f424f4d    /* "MB" */
    mov dword ptr [0xb8004], 0x4f524f45    /* "ER" */
    mov dword ptr [0xb8008], 0x4f4f4f52    /* "RO" */
    mov dword ptr [0xb800c], 0x4f214f52    /* "R!" */
    hlt

.code64
start64:
    mov ax, 0x00
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov rdi, [multiboot_magic_val]
    mov rsi, [multiboot_info_ptr]

    .extern kmain
    call kmain

    hlt

