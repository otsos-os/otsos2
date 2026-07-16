# OTSOS UEFI Bootloader

This directory contains the UEFI bootloader for OTSOS.

## Overview

The UEFI bootloader is responsible for:
1. Initializing the UEFI environment
2. Loading the OTSOS kernel from disk
3. Setting up memory management
4. Transitioning from UEFI to kernel mode
5. Passing control to the kernel

## Directory Structure

- `loader.c` - Main C code for the bootloader
- `start.asm` - Assembly entry point
- `uefi.ld` - Linker script
- `Makefile` - Build configuration
- `include/` - UEFI headers
  - `efi.h` - Basic UEFI type definitions
  - `efilib.h` - UEFI library functions
- `efilib.c` - UEFI library implementation

## Building

To build the UEFI bootloader:

```bash
make
```

This will produce `uefi_loader.efi` which can be placed in the EFI system partition.

## Requirements

- x86_64-elf-gcc (or other x86_64 cross-compiler)
- GNU binutils
- UEFI-compatible system for testing

## Installation

1. Build the bootloader: `make`
2. Copy `uefi_loader.efi` to your EFI system partition:
   ```bash
   cp uefi_loader.efi /boot/efi/EFI/otsos/
   ```
3. Create a boot entry using `efibootmgr`:
   ```bash
   sudo efibootmgr --create --disk /dev/sda --part 1 --loader /EFI/otsos/uefi_loader.efi --label "OTSOS"
   ```

## Kernel Loading

The bootloader expects the kernel to be located at:
- `/EFI/otsos/kernel.elf` (primary location)
- `/kernel.elf` (fallback location)

## Memory Map

The bootloader retrieves the UEFI memory map and passes it to the kernel.
The kernel can use this information to set up its own memory management.

## Transition to Kernel

The bootloader:
1. Exits UEFI boot services
2. Sets up page tables for long mode
3. Jumps to the kernel entry point (kmain)

## Debugging

For debugging, you can use:
- QEMU with UEFI support: `qemu-system-x86_64 -bios OVMF.fd -cdrom otsos.iso`
- Serial output (if configured)
- UEFI shell for manual testing

## Notes

- This is a minimal UEFI bootloader implementation
- For a production system, consider using EDK2 or other mature UEFI frameworks
- The bootloader currently supports only basic functionality
- More features (ACPI, SMBIOS, etc.) can be added as needed
