/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UEFI_H
#define UEFI_H

#include <mlibc/mlibc.h>

/* UEFI Bootloader Magic Number */
#define UEFI_BOOTLOADER_MAGIC 0xOTSOSUEFI

/* Boot flags */
#define BOOT_FLAG_DISABLE_APIC 0x00000001
#define BOOT_FLAG_NETWORK 0x10000000

/* UEFI Memory Types */
#define EFI_RESERVED_MEMORY_TYPE 0
#define EFI_LOADER_CODE 1
#define EFI_LOADER_DATA 2
#define EFI_BOOT_SERVICES_CODE 3
#define EFI_BOOT_SERVICES_DATA 4
#define EFI_RUNTIME_SERVICES_CODE 5
#define EFI_RUNTIME_SERVICES_DATA 6
#define EFI_CONVENTIONAL_MEMORY 7
#define EFI_UNUSABLE_MEMORY 8
#define EFI_ACPI_RECLAIM_MEMORY 9
#define EFI_ACPI_MEMORY_NVS 10
#define EFI_MEMORY_MAPPED_IO 11
#define EFI_MEMORY_MAPPED_IO_PORT_SPACE 12
#define EFI_PAL_CODE 13
#define EFI_PERSISTENT_MEMORY 14

/* UEFI Memory Descriptor */
typedef struct {
    u32 type;
    u64 physical_start;
    u64 virtual_start;
    u64 number_of_pages;
    u64 attribute;
} __attribute__((packed)) efi_memory_descriptor_t;

/* UEFI Boot Information Structure */
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
    u64 kernel_physical_addr;
    u64 kernel_size;
    u64 boot_option;
    u64 boot_flags;
} __attribute__((packed)) uefi_boot_info_t;

/* UEFI Framebuffer Information */
typedef struct {
    u64 base_address;
    u32 width;
    u32 height;
    u32 pitch;
    u32 bpp;
    u32 type;
} __attribute__((packed)) uefi_framebuffer_info_t;

/* Function prototypes */
void bootmem_init_uefi(uefi_boot_info_t *uefi_info);
void drm_boot_init_uefi(uefi_boot_info_t *uefi_info);
void acpi_init_from_uefi(uefi_boot_info_t *uefi_info);

#endif /* UEFI_H */
