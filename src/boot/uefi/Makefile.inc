# Common includes for UEFI bootloader

# Architecture
ARCH ?= x86_64

# Compiler prefix
CROSS_COMPILE ?= x86_64-elf-

# Tools
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJDUMP = $(CROSS_COMPILE)objdump

# Include paths
INC = -Iinclude -I.

# Compiler flags
CFLAGS = -m64 -fno-stack-protector -fno-builtin -nostdlib -nostdinc \
	-D__UEFI__ -D__$(ARCH)__ -fshort-wchar \
	-Wall -Wextra -O2 $(INC)

# Assembler flags
ASFLAGS = --64 $(INC)

# Linker flags
LDFLAGS = -m elf_x86_64 -T uefi.ld -nostdlib

# Output format
TARGET_EXT = .efi
