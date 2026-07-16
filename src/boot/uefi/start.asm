/*
 * Copyright (c) 2026, otsos team
 *
 * UEFI Entry Point for OTSOS
 *
 * This is the entry point for the UEFI bootloader.
 * It sets up the initial environment and calls the C entry point.
 */

.intel_syntax noprefix

// UEFI calling convention: first 4 integer/class parameters in RCX, RDX, R8, R9
// Stack must be 16-byte aligned

.section .text
.code64

.global efimain
.type efimain, @function

// Entry point from UEFI firmware
efimain:
    // Save the image handle and system table pointers
    mov [image_handle_ptr], rcx
    mov [system_table_ptr], rdx
    
    // Set up stack pointer
    // UEFI requires 16-byte alignment
    // We'll use a stack allocated in the data section
    lea rsp, [stack_top]
    
    // Call the C entry point
    // efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table)
    mov rcx, [image_handle_ptr]
    mov rdx, [system_table_ptr]
    call efi_main
    
    // Return status to UEFI
    ret

// Global pointers for image handle and system table
.section .data
.image_handle_ptr:
    .quad 0
.system_table_ptr:
    .quad 0

// Stack for the bootloader
.section .bss
.align 16
stack_bottom:
    .skip 65536  // 64KB stack
stack_top:

// End of file
