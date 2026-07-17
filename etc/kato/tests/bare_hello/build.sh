#!/bin/bash
set -e

cd "$(dirname "$0")"
KATO_ROOT=../..
PYTHON=python3
CC=cc
AS=as
LD=ld
OBJCOPY=objcopy
QEMU=qemu-system-i386

echo "=== Compiling kernel.kato -> C ==="
$PYTHON "$KATO_ROOT/src/main.py" kernel.kato -freestand -o kernel.c

echo "=== Compiling kernel.c -> object (32-bit freestanding) ==="
$CC -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector \
    -Wall -Wextra -c kernel.c -o kernel.o

echo "=== Assembling boot.s ==="
$AS --32 boot.s -o boot.o

echo "=== Linking boot.o + kernel.o -> kernel.elf ==="
$LD -m elf_i386 -T linker.ld --no-dynamic-linker --build-id=none \
    boot.o kernel.o -o kernel.elf

echo "=== Extracting raw binary ==="
$OBJCOPY -O binary kernel.elf kernel.bin

echo "=== Padding to floppy image ==="
dd if=/dev/zero of=floppy.img bs=512 count=2880 2>/dev/null
dd if=kernel.bin of=floppy.img conv=notrunc 2>/dev/null

echo "=== Size ==="
ls -l kernel.bin floppy.img
wc -c kernel.bin

echo ""
echo "Done. Run with:"
echo "  $QEMU -drive format=raw,file=floppy.img,if=floppy -nographic"
