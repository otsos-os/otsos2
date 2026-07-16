# UEFI Bootloader Integration Guide for OTSOS

## Overview

This document describes how to integrate the UEFI bootloader with OTSOS.

## Current State

OTSOS currently uses **Multiboot2** (GRUB) for booting. The new UEFI bootloader provides:

1. **Direct UEFI boot** without GRUB
2. **Secure Boot** compatibility (future)
3. **Modern hardware support**
4. **Faster boot times**

## Architecture

### Boot Flow

```
UEFI Firmware
    ↓
uefi_loader.efi (UEFI Bootloader)
    ↓
1. Initialize UEFI environment
2. Get memory map
3. Load kernel from disk
4. Set up page tables
5. Exit boot services
6. Jump to kernel (kmain)
    ↓
OTSOS Kernel
```

### Kernel Entry Point

The UEFI bootloader passes control to the kernel at the `kmain` function with the following parameters:

```c
void kmain(
    u64 magic,        // Boot magic (for compatibility)
    u64 addr,         // Pointer to boot information
    u64 boot_option,  // Boot options (0=normal, 1=safe, 2=debug)
    u64 boot_flags    // Additional flags
);
```

## Integration Steps

### 1. Modify Kernel Entry Point

The kernel needs to handle both Multiboot and UEFI entry points. Update `src/kernel/kernel.c`:

```c
// Add UEFI magic number
#define UEFI_BOOTLOADER_MAGIC 0xOTSOSUEFI

// In kmain, add UEFI detection
void kmain(u64 magic, u64 addr, u64 boot_option, u64 boot_flags)
{
    // ... existing code ...
    
    if (magic == UEFI_BOOTLOADER_MAGIC) {
        // UEFI boot path
        uefi_init(addr);  // Initialize UEFI-specific features
    } else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        // Multiboot2 boot path (existing)
        is_multiboot2 = 1;
        // ... existing code ...
    }
    // ... rest of initialization ...
}
```

### 2. Update Linker Script

The UEFI bootloader needs to be compiled as a **position-independent EFI application**. 
The existing `src/linker.ld` is for the kernel, not the bootloader.

### 3. Memory Management

UEFI provides a memory map that the kernel can use. Update `src/kernel/bootmem.c`:

```c
// Add UEFI memory map parsing
void bootmem_init_uefi(void *uefi_mem_map)
{
    // Parse UEFI memory map
    // Reserve UEFI runtime memory
    // Initialize physical memory allocator
}
```

### 4. Framebuffer Initialization

UEFI provides GOP (Graphics Output Protocol) for framebuffer access:

```c
// In drm_boot_init_mb2, add UEFI path
void drm_boot_init_uefi(void *uefi_info)
{
    // Get framebuffer from UEFI GOP
    // Set up initial framebuffer for kernel
}
```

### 5. ACPI Tables

UEFI provides direct access to ACPI tables:

```c
// In acpi_init_from_multiboot2, add UEFI path
void acpi_init_from_uefi(void *uefi_info)
{
    // Get ACPI tables from UEFI
    // Parse and install ACPI handlers
}
```

## Building

### Prerequisites

1. **x86_64-elf-gcc** toolchain
2. **GNU binutils**
3. **UEFI development environment** (optional for testing)

### Build Commands

```bash
# Build UEFI bootloader
cd src/boot/uefi
make

# Or use the build script
./build_uefi.sh
```

### Output

The build produces `uefi_loader.efi` which is a **UEFI application** that can be:
- Placed in the EFI System Partition (ESP)
- Loaded directly by UEFI firmware
- Used with `efibootmgr` to create boot entries

## Testing

### QEMU Testing

```bash
# Install OVMF (UEFI firmware for QEMU)
sudo apt-get install ovmf

# Run QEMU with UEFI
qemu-system-x86_64 \
    -bios /usr/share/OVMF/OVMF.fd \
    -drive if=pflash,format=raw,file=/usr/share/OVMF/OVMF_VARS.fd \
    -cdrom otsos.iso \
    -m 4G \
    -serial stdio
```

### Real Hardware Testing

1. Copy `uefi_loader.efi` to ESP:
   ```bash
   sudo cp uefi_loader.efi /boot/efi/EFI/otsos/
   ```

2. Create boot entry:
   ```bash
   sudo efibootmgr --create --disk /dev/sda --part 1 \
       --loader /EFI/otsos/uefi_loader.efi \
       --label "OTSOS" \
       --bootnum
   ```

3. Set as default:
   ```bash
   sudo efibootmgr --bootorder XXXX,YYYY --verbose
   ```

## Configuration

The UEFI bootloader can be configured through:

1. **Boot options** in the UEFI boot manager
2. **Configuration file** (`/EFI/otsos/config.toml`)
3. **Command line parameters**

### Example Configuration

```toml
# /EFI/otsos/config.toml
[boot]
safe_mode = false
debug = false
disable_apic = false

[kernel]
path = "/EFI/otsos/kernel.elf"

[memory]
reserve_uefi_runtime = true
```

## Kernel Modifications Required

### 1. Entry Point Detection

Update `src/kernel/kernel.c:kmain()` to detect UEFI boot:

```c
#define UEFI_BOOTLOADER_MAGIC 0xOTSOSUEFI

void kmain(u64 magic, u64 addr, u64 boot_option, u64 boot_flags)
{
    if (magic == UEFI_BOOTLOADER_MAGIC) {
        // UEFI boot
        uefi_boot_info_t *uefi_info = (uefi_boot_info_t *)addr;
        bootmem_init_uefi(uefi_info);
        drm_boot_init_uefi(uefi_info);
        acpi_init_from_uefi(uefi_info);
    } else if (magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        // Multiboot2 boot (existing)
        // ...
    }
    // ... rest of initialization
}
```

### 2. UEFI Information Structure

Add to `src/kernel/multiboot2.h`:

```c
typedef struct {
    u64 memory_map_addr;
    u64 memory_map_size;
    u64 memory_map_descriptor_size;
    u64 framebuffer_addr;
    u32 framebuffer_width;
    u32 framebuffer_height;
    u32 framebuffer_pitch;
    u32 framebuffer_bpp;
    u64 acpi_rsdp_addr;
    // ... other UEFI-specific info
} uefi_boot_info_t;
```

### 3. Memory Map Parsing

Add to `src/kernel/bootmem.c`:

```c
void bootmem_init_uefi(uefi_boot_info_t *uefi_info)
{
    EFI_MEMORY_DESCRIPTOR *mem_map = (EFI_MEMORY_DESCRIPTOR *)uefi_info->memory_map_addr;
    u64 mem_map_end = uefi_info->memory_map_addr + uefi_info->memory_map_size;
    
    while ((u8 *)mem_map < (u8 *)mem_map_end) {
        if (mem_map->Type == EfiConventionalMemory) {
            u64 start = mem_map->PhysicalStart;
            u64 pages = mem_map->NumberOfPages;
            u64 size = pages * PAGE_SIZE;
            
            // Add to bootmem
            bootmem_add_region(start, size);
        }
        
        mem_map = (EFI_MEMORY_DESCRIPTOR *)((u8 *)mem_map + uefi_info->memory_map_descriptor_size);
    }
}
```

## Boot Options

The UEFI bootloader supports the following boot options:

| Option | Value | Description |
|--------|-------|-------------|
| Normal | 0 | Standard boot |
| Safe Mode | 1 | Minimal initialization |
| Debug | 2 | Verbose output, no userspace |

## Error Handling

The bootloader provides error messages through:

1. **UEFI console output** (ConOut)
2. **Serial port** (if available)
3. **Error codes** returned to UEFI firmware

### Common Errors

- `EFI_LOAD_ERROR`: Failed to load kernel file
- `EFI_NOT_FOUND`: Kernel file not found
- `EFI_OUT_OF_RESOURCES`: Memory allocation failed
- `EFI_INVALID_PARAMETER`: Invalid configuration

## Future Enhancements

1. **Secure Boot support**
2. **Network boot** (PXE)
3. **File system support** (ext4, FAT32)
4. **Boot menu** with configuration options
5. **Kernel command line** parsing
6. **Initrd support**
7. **Memory compression** for larger kernels

## Troubleshooting

### Bootloader doesn't appear in UEFI menu

1. Check if file is in the correct location:
   ```bash
   ls /boot/efi/EFI/otsos/
   ```
2. Verify boot entry:
   ```bash
   sudo efibootmgr -v
   ```
3. Check file signature:
   ```bash
   file uefi_loader.efi
   ```
   Should show: `PE32+ executable (EFI application, x86-64)`

### Kernel fails to load

1. Check kernel path in bootloader
2. Verify kernel is compiled for UEFI boot
3. Check memory map is being passed correctly

### Black screen after bootloader

1. Check if framebuffer information is passed correctly
2. Verify page tables are set up properly
3. Check if kernel is receiving correct parameters

## References

- [UEFI Specification](https://uefi.org/specifications)
- [EDK2 Project](https://github.com/tianocore/edk2)
- [GNU-EFI](https://github.com/vathpela/gnu-efi)
- [OSDev Wiki - UEFI](https://wiki.osdev.org/UEFI)

## License

This bootloader is licensed under the same BSD 2-Clause license as OTSOS.
