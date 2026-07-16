ENTRY(stage2_entry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS
{
	. = 0x8000;

	.entry16 : {
		*(.entry16)
		*(.bios16data)
	}

	. = ALIGN(16);
	.text : {
		*(.text)
		*(.text.*)
	}

	.rodata : {
		*(.rodata)
		*(.rodata.*)
	}

	.data : {
		*(.data)
		*(.data.*)
	}

	.bss : {
		*(.bss)
		*(.bss.*)
		*(COMMON)
	}

	. = ALIGN(16);
	stage2_end = .;

	/DISCARD/ : {
		*(.comment)
		*(.note*)
		*(.eh_frame*)
	}
}
