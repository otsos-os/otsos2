ENTRY(start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
    . = 1M;

    .text :
    {
        *(.multiboot)
        *(.text)
    }

    .rodata : { *(.rodata) }
    .data   : { *(.data) }
    .bss    : { 
        *(.bss*) 
        *(COMMON) 
    }
    . = ALIGN(65536);
    kernel_end = .;
}
