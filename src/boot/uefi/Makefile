# Makefile for OTSOS UEFI Bootloader

include Makefile.inc

# Source files
SRCS = loader.c efilib.c network.c
ASM_SRCS = start.asm
OBJS = $(SRCS:.c=.o) $(ASM_SRCS:.asm=.o)

# Output file
TARGET = uefi_loader

all: $(TARGET)$(TARGET_EXT)

# C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Assembly source files
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Link everything
$(TARGET).elf: $(OBJS) uefi.ld
	$(LD) $(LDFLAGS) -o $@ $^

# Convert to UEFI application
$(TARGET)$(TARGET_EXT): $(TARGET).elf
	$(OBJCOPY) \
		-j .text -j .sdata -j .data -j .dynamic \
		-j .dynsym -j .rel.dyn -j .rel.text \
		-j .rel.sdata -j .rel.data -j .rel.dynamic \
		--target efi-app-x86_64 $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET)$(TARGET_EXT)

.PHONY: all clean
