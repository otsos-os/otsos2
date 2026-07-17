.code16
.section .boot, "ax"
.global _start
_start:
    cli
    xor    %ax, %ax
    mov    %ax, %ds
    mov    %ax, %es
    mov    %ax, %ss
    mov    $0x7C00, %sp

    xor    %ah, %ah
    xor    %dl, %dl
    int    $0x13

    mov    $0x02, %ah
    mov    $0x02, %al
    mov    $0x0002, %cx
    xor    %dx, %dx
    mov    $0x7E00, %bx
    int    $0x13
    jc     disk_err

    lgdt   gdt_descriptor

    in     $0x92, %al
    or     $2, %al
    out    %al, $0x92

    mov    %cr0, %eax
    or     $1, %eax
    mov    %eax, %cr0

    ljmp   $CODE_SEG, $protected_mode

disk_err:
    hlt
    jmp    disk_err

.code32
protected_mode:
    mov    $DATA_SEG, %ax
    mov    %ax, %ds
    mov    %ax, %es
    mov    %ax, %fs
    mov    %ax, %gs
    mov    %ax, %ss
    mov    $0x90000, %esp

    call   main

    cli
1:  hlt
    jmp    1b

.align 8
gdt_start:
gdt_null:
    .quad  0
gdt_code:
    .word  0xFFFF
    .word  0
    .byte  0
    .byte  0x9A
    .byte  0xCF
    .byte  0
gdt_data:
    .word  0xFFFF
    .word  0
    .byte  0
    .byte  0x92
    .byte  0xCF
    .byte  0
gdt_end:

gdt_descriptor:
    .word  gdt_end - gdt_start - 1
    .long  gdt_start

.equ CODE_SEG, gdt_code - gdt_start
.equ DATA_SEG, gdt_data - gdt_start

.fill 510 - (. - _start), 1, 0
.word 0xAA55
