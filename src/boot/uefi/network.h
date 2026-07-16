/*
 * Copyright (c) 2026, otsos team
 *
 * UEFI Network Boot Support for OTSOS
 *
 * This file provides network boot (PXE) functionality for the UEFI bootloader.
 */

#ifndef UEFI_NETWORK_H
#define UEFI_NETWORK_H

#include <efi.h>

// UEFI Network Protocols GUIDs
#define EFI_SIMPLE_NETWORK_PROTOCOL_GUID \
    {0x3C539743, 0x817E, 0x4C5E, {0xB2, 0x29, 0x01, 0x4C, 0x87, 0x8B, 0x8C, 0xE2}}

#define EFI_PXE_BASE_CODE_PROTOCOL_GUID \
    {0x03C4E658, 0xAC23, 0x4864, {0x86, 0x40, 0xF6, 0x87, 0x4F, 0x87, 0xBA, 0x84}}

#define EFI_LOAD_FILE_PROTOCOL_GUID \
    {0x56EC3092, 0x954C, 0x11D2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

#define EFI_LOAD_FILE2_PROTOCOL_GUID \
    {0x4006C0C1, 0xFC49, 0x47E0, {0xA9, 0x74, 0x12, 0x76, 0x98, 0x90, 0x62, 0x93}}

// Network boot configuration
#define NETWORK_BOOT_TIMEOUT 30  // seconds
#define NETWORK_RETRY_COUNT 3

// DHCP options
#define DHCP_OPTION_SUBNET_MASK 1
#define DHCP_OPTION_ROUTER 3
#define DHCP_OPTION_DNS 6
#define DHCP_OPTION_HOSTNAME 12
#define DHCP_OPTION_DOMAIN_NAME 15
#define DHCP_OPTION_ROOT_PATH 17
#define DHCP_OPTION_TFTP_SERVER 66
#define DHCP_OPTION_BOOTFILE 67

// TFTP opcodes
#define TFTP_RRQ 0
#define TFTP_WRQ 1
#define TFTP_DATA 3
#define TFTP_ACK 4
#define TFTP_ERROR 5

// TFTP block size
#define TFTP_BLOCK_SIZE 512

// Network boot file names
#define DEFAULT_BOOTFILE "otsos/kernel.elf"
#define DEFAULT_CONFIGFILE "otsos/config.toml"

// Network interface structure
typedef struct {
    EFI_HANDLE handle;
    EFI_SIMPLE_NETWORK_PROTOCOL *simple_network;
    EFI_PXE_BASE_CODE_PROTOCOL *pxe;
    EFI_LOAD_FILE_PROTOCOL *load_file;
    EFI_LOAD_FILE2_PROTOCOL *load_file2;
    
    UINT8 mac_addr[32];
    UINT32 mac_addr_len;
    UINT32 ip_addr;
    UINT32 subnet_mask;
    UINT32 gateway;
    UINT32 dns;
    UINT32 server_ip;
    CHAR16 bootfile[256];
    CHAR16 tftp_server[256];
    
    UINT8 use_dhcp;
    UINT8 initialized;
} network_interface_t;

// Network boot functions
EFI_STATUS network_init(void);
EFI_STATUS network_discover_interfaces(void);
EFI_STATUS network_configure_dhcp(network_interface_t *netif);
EFI_STATUS network_load_file(network_interface_t *netif, CHAR16 *filename, VOID **buffer, UINTN *size);
EFI_STATUS network_download_kernel(VOID **kernel_buffer, UINTN *kernel_size);
EFI_STATUS network_download_config(VOID **config_buffer, UINTN *config_size);

// Utility functions
EFI_STATUS network_get_mac_address(network_interface_t *netif);
EFI_STATUS network_get_ip_config(network_interface_t *netif);
VOID network_dump_info(network_interface_t *netif);

#endif /* UEFI_NETWORK_H */
