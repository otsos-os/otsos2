CC = clang
LD = ld.lld

CFLAGS = -m64 -ffreestanding -fno-pic -fno-stack-protector -nostdlib -nostdinc \
         -fno-builtin -O0 -Wall -Wextra

LDFLAGS = -m elf_x86_64 -T ../init/linker.ld --oformat=elf64-x86-64 -nostdlib

YES_BIN = yes
YES_OBJS = yes.o

all: $(YES_BIN)

$(YES_BIN): $(YES_OBJS)
	@echo "  LLD      $@"
	@$(LD) $(LDFLAGS) $(YES_OBJS) -o $@
	@echo "  SIZE    $@"
	@size $@

%.o: %.c
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(YES_OBJS) $(YES_BIN)

.PHONY: all clean
