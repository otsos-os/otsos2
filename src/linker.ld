ENTRY(start)
OUTPUT_FORMAT(elf64-x86-64)

KERNEL_VMA = 0xFFFFFFFF80000000;

SECTIONS
{
    . = 0x100000;

    .boot : AT(0x100000) {
        *(.multiboot)
        *(.boot.text)
        *(.boot.text.*)
        *(.boot.rodata)
        *(.boot.rodata.*)
        *(.boot.data)
        *(.boot.data.*)
        *(.boot.bss)
        *(.boot.bss.*)
    }
    . = ALIGN(4096);
    boot_section_end = .;

    . = KERNEL_VMA + boot_section_end;

    .text : AT(ADDR(.text) - KERNEL_VMA) {
        *(.text)
        *(.text.*)
    }
    .rodata : AT(ADDR(.rodata) - KERNEL_VMA) {
        *(.rodata)
        *(.rodata.*)
    }
    .newbus_drivers : AT(ADDR(.newbus_drivers) - KERNEL_VMA) {
        . = ALIGN(8);
        __start_newbus_drivers = .;
        KEEP(*(.newbus.drivers))
        . = ALIGN(8);
        __stop_newbus_drivers = .;
    }
    .data : AT(ADDR(.data) - KERNEL_VMA) {
        *(.data)
        *(.data.*)
    }
    .bss : AT(ADDR(.bss) - KERNEL_VMA) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }
    . = ALIGN(65536);
    kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
