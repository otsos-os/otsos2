/*
 * Copyright (c) 2026, otsos team
 *
 * UEFI Bootloader for OTSOS
 *
 * This file is part of OTSOS - Obviously The Slowest Operating System
 */

#include <efi.h>
#include <efilib.h>
#include "network.h"

// Kernel entry point signature
#define KERNEL_VMA 0xFFFFFFFF80000000
#define UEFI_BOOTLOADER_MAGIC 0xOTSOSUEFI

// Memory map related
#define MAX_MEMORY_MAP_ENTRIES 128
#define PAGE_SIZE 4096

// Global for selected boot option
static UINTN selected_item = 0;

// Boot source
#define BOOT_SOURCE_LOCAL 0
#define BOOT_SOURCE_NETWORK 1

// Boot configuration
static UINTN g_boot_source = BOOT_SOURCE_LOCAL;

// UEFI Boot Info structure (must match kernel's uefi.h)
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
} uefi_boot_info_t;

typedef struct {
    EFI_MEMORY_DESCRIPTOR *map;
    UINTN map_size;
    UINTN map_key;
    UINTN descriptor_size;
    UINT32 descriptor_version;
} MemoryMap;

typedef struct {
    EFI_PHYSICAL_ADDRESS addr;
    UINT64 size;
} KernelSection;

// Global variables
EFI_SYSTEM_TABLE *ST;
EFI_BOOT_SERVICES *BS;
EFI_RUNTIME_SERVICES *RT;

// Function prototypes
EFI_STATUS efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table);
EFI_STATUS get_memory_map(MemoryMap *mem_map);
EFI_STATUS exit_boot_services(EFI_HANDLE image_handle, MemoryMap *mem_map);
EFI_STATUS load_kernel(EFI_FILE_HANDLE root_dir, KernelSection *kernel_sections, UINTN *section_count);
EFI_STATUS find_kernel(EFI_FILE_HANDLE *root_dir, EFI_FILE_HANDLE *kernel_file);
VOID *allocate_pages(UINTN pages, EFI_MEMORY_TYPE type);
EFI_STATUS set_up_page_tables(MemoryMap *mem_map);

// Entry point
EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
{
    EFI_STATUS status;
    MemoryMap mem_map = {0};
    EFI_FILE_HANDLE root_dir = NULL;
    EFI_FILE_HANDLE kernel_file = NULL;
    KernelSection kernel_sections[16];
    UINTN section_count = 0;
    
    ST = system_table;
    BS = ST->BootServices;
    RT = ST->RuntimeServices;
    
    // Initialize console
    ST->ConOut->Reset(ST->ConOut, FALSE);
    ST->ConOut->ClearScreen(ST->ConOut);
    
    Print(L"OTSOS UEFI Bootloader\r\n");
    Print(L"Initializing...\r\n");
    
    // Check if we should try network boot
    Print(L"Trying network boot...\r\n");
    status = network_init();
    if (!EFI_ERROR(status)) {
        g_boot_source = BOOT_SOURCE_NETWORK;
        Print(L"Network boot available\r\n");
    } else {
        Print(L"Network boot not available, using local\r\n");
        g_boot_source = BOOT_SOURCE_LOCAL;
    }
    
    // Get memory map
    status = get_memory_map(&mem_map);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get memory map: %r\r\n", status);
        return status;
    }
    
    Print(L"Memory map obtained\r\n");
    
    // Load kernel based on boot source
    if (g_boot_source == BOOT_SOURCE_NETWORK) {
        Print(L"Attempting network kernel download...\r\n");
        status = network_download_kernel(&kernel_sections[0].addr, &kernel_sections[0].size);
        if (EFI_ERROR(status)) {
            Print(L"WARNING: Network download failed, trying local: %r\r\n", status);
            // Fall back to local
            g_boot_source = BOOT_SOURCE_LOCAL;
        } else {
            section_count = 1;
            Print(L"Kernel downloaded via network: %u bytes\r\n", kernel_sections[0].size);
        }
    }
    
    // If network failed or not available, try local
    if (g_boot_source == BOOT_SOURCE_LOCAL) {
        EFI_FILE_HANDLE root_dir = NULL;
        EFI_FILE_HANDLE kernel_file = NULL;
        
        // Find kernel
        status = find_kernel(&root_dir, &kernel_file);
        if (EFI_ERROR(status)) {
            Print(L"ERROR: Failed to find kernel: %r\r\n", status);
            return status;
        }
        
        Print(L"Kernel found\r\n");
        
        // Load kernel sections
        status = load_kernel(root_dir, kernel_sections, &section_count);
        if (EFI_ERROR(status)) {
            Print(L"ERROR: Failed to load kernel: %r\r\n", status);
            return status;
        }
        
        Print(L"Kernel loaded from local storage: %u bytes\r\n", kernel_sections[0].size);
    }
    
    Print(L"Kernel ready at 0x%lx, %d sections\r\n", kernel_sections[0].addr, section_count);
    
    // Set up page tables for long mode
    status = set_up_page_tables(&mem_map);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to set up page tables: %r\r\n", status);
        return status;
    }
    
    Print(L"Page tables set up\r\n");
    
    // Exit boot services
    status = exit_boot_services(image_handle, &mem_map);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to exit boot services: %r\r\n", status);
        return status;
    }
    
    Print(L"Boot services exited\r\n");
    
    // Now we're in a tricky situation - we need to transition to kernel
    // This is where we would jump to the kernel entry point
    // For now, we'll just hang
    // Exit boot services before jumping to kernel
    status = exit_boot_services(image_handle, &mem_map);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to exit boot services: %r\r\n", status);
        return status;
    }
    
    Print(L"Ready to jump to kernel...\r\n");
    
    // Set up kernel entry point and parameters
    u64 magic = UEFI_BOOTLOADER_MAGIC;
    u64 boot_option = 0;  // Default to normal boot
    u64 boot_flags = 0;  // No special flags
    
    // Prepare UEFI boot info structure
    uefi_boot_info_t uefi_info;
    uefi_info.memory_map_addr = (u64)mem_map.map;
    uefi_info.memory_map_size = mem_map.map_size;
    uefi_info.memory_map_descriptor_size = mem_map.descriptor_size;
    uefi_info.framebuffer_addr = 0;  // Will be filled if available
    uefi_info.framebuffer_width = 0;
    uefi_info.framebuffer_height = 0;
    uefi_info.framebuffer_pitch = 0;
    uefi_info.framebuffer_bpp = 0;
    uefi_info.acpi_rsdp_addr = 0;  // Will be filled if available
    uefi_info.kernel_physical_addr = kernel_sections[0].addr;
    uefi_info.kernel_size = kernel_sections[0].size;
    uefi_info.boot_option = boot_option;
    uefi_info.boot_flags = boot_flags;
    
    // Set boot source flag in boot_flags
    if (g_boot_source == BOOT_SOURCE_NETWORK) {
        uefi_info.boot_flags |= 0x10000000;  // BOOT_FLAG_NETWORK
    }
    
    // For now, we'll use simplified approach
    // In a full implementation, we would use GOP and ACPI protocols
    
    // Try to download config if network boot
    VOID *config_buffer = NULL;
    UINTN config_size = 0;
    if (g_boot_source == BOOT_SOURCE_NETWORK) {
        network_download_config(&config_buffer, &config_size);
    }
    
    // Try to get framebuffer from UEFI (simplified)
    // Try to get ACPI RSDP from UEFI (simplified)
    
    // Set up stack for kernel
    // Stack will be at the end of kernel memory + 1MB
    u64 kernel_stack_top = kernel_sections[0].addr + kernel_sections[0].size + 0x100000;
    
    // Jump to kernel in assembly
    // kmain is at KERNEL_VMA + offset
    // For now, we'll use a simple jump to the kernel entry
    __asm__ volatile (
        "mov %[magic], %%rdi\n\t"
        "mov %[addr], %%rsi\n\t"
        "mov %[boot_option], %%rdx\n\t"
        "mov %[boot_flags], %%rcx\n\t"
        "mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %[stack_top], %%rsp\n\t"
        "jmp *%[kernel_entry]\n\t"
        :
        : [magic] "r" (magic),
          [addr] "r" (&uefi_info),
          [boot_option] "r" (boot_option),
          [boot_flags] "r" (boot_flags),
          [stack_top] "r" (kernel_stack_top),
          [kernel_entry] "r" (KERNEL_VMA)
        : "memory", "cc"
    );
    
    // Should never reach here
    while (1) {
        __asm__("hlt");
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
get_memory_map(MemoryMap *mem_map)
{
    EFI_STATUS status;
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    UINT32 descriptor_version = 0;
    
    // First call to get the required buffer size
    status = BS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    // Allocate buffer for memory map
    mem_map->map_size = map_size + PAGE_SIZE;
    mem_map->map = (EFI_MEMORY_DESCRIPTOR *)allocate_pages(
        EFI_SIZE_TO_PAGES(mem_map->map_size), 
        EfiLoaderData
    );
    
    if (!mem_map->map) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Second call to actually get the memory map
    status = BS->GetMemoryMap(
        &mem_map->map_size, 
        mem_map->map, 
        &mem_map->map_key, 
        &mem_map->descriptor_size, 
        &mem_map->descriptor_version
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    mem_map->descriptor_size = descriptor_size;
    mem_map->descriptor_version = descriptor_version;
    
    return EFI_SUCCESS;
}

EFI_STATUS
exit_boot_services(EFI_HANDLE image_handle, MemoryMap *mem_map)
{
    EFI_STATUS status;
    UINTN map_key = mem_map->map_key;
    UINTN map_size = mem_map->map_size;
    UINTN descriptor_version = mem_map->descriptor_version;
    
    // Exit boot services
    status = BS->ExitBootServices(image_handle, map_key);
    
    // After exiting boot services, the memory map might have changed
    // We need to get a new memory map
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Get new memory map
    status = RT->GetMemoryMap(
        &map_size,
        mem_map->map,
        &map_key,
        &mem_map->descriptor_size,
        &descriptor_version
    );
    
    return status;
}

VOID *
allocate_pages(UINTN pages, EFI_MEMORY_TYPE type)
{
    EFI_PHYSICAL_ADDRESS addr = 0;
    EFI_STATUS status;
    
    status = BS->AllocatePages(
        AllocateAnyPages,
        type,
        pages,
        &addr
    );
    
    if (EFI_ERROR(status)) {
        return NULL;
    }
    
    return (VOID *)(UINTN)addr;
}

EFI_STATUS
find_kernel(EFI_FILE_HANDLE *root_dir, EFI_FILE_HANDLE *kernel_file)
{
    EFI_STATUS status;
    EFI_LOADED_IMAGE_PROTOCOL *loaded_image = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs = NULL;
    EFI_FILE_HANDLE volume = NULL;
    
    // Get the loaded image protocol
    status = BS->HandleProtocol(
        BS->GetHandleForImage(BS->GetCurrentImage()),
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&loaded_image
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Get the file system protocol
    status = BS->HandleProtocol(
        loaded_image->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&fs
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Open the volume
    status = fs->OpenVolume(fs, &volume);
    if (EFI_ERROR(status)) {
        return status;
    }
    
    *root_dir = volume;
    
    // Try to open the kernel file
    status = volume->Open(
        volume,
        kernel_file,
        L"\\EFI\\otsos\\kernel.elf",
        EFI_FILE_MODE_READ,
        0
    );
    
    if (EFI_ERROR(status)) {
        // Try alternative path
        status = volume->Open(
            volume,
            kernel_file,
            L"\\kernel.elf",
            EFI_FILE_MODE_READ,
            0
        );
        
        if (EFI_ERROR(status)) {
            return status;
        }
    }
    
    return EFI_SUCCESS;
}

EFI_STATUS
load_kernel(EFI_FILE_HANDLE root_dir, KernelSection *kernel_sections, UINTN *section_count)
{
    EFI_STATUS status;
    EFI_FILE_HANDLE kernel_file = NULL;
    EFI_FILE_INFO *file_info = NULL;
    UINTN file_info_size = 0;
    UINTN buffer_size = 0;
    VOID *buffer = NULL;
    
    // Open kernel file
    status = root_dir->Open(
        root_dir,
        &kernel_file,
        L"\\EFI\\otsos\\kernel.elf",
        EFI_FILE_MODE_READ,
        0
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Get file info to determine size
    file_info_size = 0;
    status = kernel_file->GetInfo(
        kernel_file,
        &gEfiFileInfoGuid,
        &file_info_size,
        NULL
    );
    
    if (status != EFI_BUFFER_TOO_SMALL) {
        return status;
    }
    
    // Allocate buffer for file info
    file_info = (EFI_FILE_INFO *)allocate_pages(
        EFI_SIZE_TO_PAGES(file_info_size),
        EfiLoaderData
    );
    
    if (!file_info) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    status = kernel_file->GetInfo(
        kernel_file,
        &gEfiFileInfoGuid,
        &file_info_size,
        file_info
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // Allocate buffer for kernel
    buffer_size = file_info->FileSize + PAGE_SIZE;
    buffer = allocate_pages(
        EFI_SIZE_TO_PAGES(buffer_size),
        EfiLoaderCode
    );
    
    if (!buffer) {
        return EFI_OUT_OF_RESOURCES;
    }
    
    // Read kernel file
    status = kernel_file->Read(
        kernel_file,
        &file_info->FileSize,
        buffer
    );
    
    if (EFI_ERROR(status)) {
        return status;
    }
    
    // For now, just store the kernel address and size
    // In a real implementation, we would parse the ELF file
    kernel_sections[0].addr = (EFI_PHYSICAL_ADDRESS)(UINTN)buffer;
    kernel_sections[0].size = file_info->FileSize;
    *section_count = 1;
    
    Print(L"Kernel loaded: %d bytes at 0x%lx\r\n", file_info->FileSize, buffer);
    
    return EFI_SUCCESS;
}

EFI_STATUS
set_up_page_tables(MemoryMap *mem_map)
{
    // This is a placeholder for page table setup
    // In a real implementation, we would:
    // 1. Parse the memory map to find available memory
    // 2. Set up identity mapping for the first few GB
    // 3. Set up higher-half mapping for the kernel
    // 4. Enable paging
    
    Print(L"Setting up page tables...\r\n");
    
    // For now, just return success
    return EFI_SUCCESS;
}
