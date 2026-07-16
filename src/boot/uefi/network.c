/*
 * Copyright (c) 2026, otsos team
 *
 * UEFI Network Boot (PXE) Implementation for OTSOS
 *
 * This file implements network boot functionality using UEFI protocols.
 */

#include "network.h"
#include "efilib.h"

// Global network interface
static network_interface_t g_network_interfaces[16];
static UINTN g_network_interface_count = 0;
static network_interface_t *g_selected_interface = NULL;

// DHCP state
static UINT8 g_dhcp_discovered = 0;
static UINT32 g_dhcp_server_ip = 0;
static CHAR16 g_dhcp_bootfile[256] = {0};
static CHAR16 g_dhcp_tftp_server[256] = {0};

// TFTP state
static UINT16 g_tftp_block = 1;

//
// Network Initialization
//

EFI_STATUS
network_init(void)
{
    EFI_STATUS status;
    
    Print(L"Initializing network subsystem...\r\n");
    
    // Discover all network interfaces
    status = network_discover_interfaces();
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to discover network interfaces: %r\r\n", status);
        return status;
    }
    
    if (g_network_interface_count == 0) {
        Print(L"ERROR: No network interfaces found\r\n");
        return EFI_NOT_FOUND;
    }
    
    // Select first interface for now
    g_selected_interface = &g_network_interfaces[0];
    
    // Initialize the interface
    status = network_get_mac_address(g_selected_interface);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get MAC address: %r\r\n", status);
        return status;
    }
    
    // Try DHCP
    status = network_configure_dhcp(g_selected_interface);
    if (EFI_ERROR(status)) {
        Print(L"WARNING: DHCP failed, using static configuration\r\n");
        // For now, we'll continue without DHCP
        g_selected_interface->use_dhcp = 0;
    } else {
        g_selected_interface->use_dhcp = 1;
    }
    
    network_dump_info(g_selected_interface);
    
    Print(L"Network initialized\r\n");
    return EFI_SUCCESS;
}

//
// Network Interface Discovery
//

EFI_STATUS
network_discover_interfaces(void)
{
    EFI_STATUS status;
    EFI_HANDLE *handles = NULL;
    UINTN handle_count = 0;
    UINTN i;
    
    Print(L"Discovering network interfaces...\r\n");
    
    // Get all handles that support Simple Network Protocol
    status = BS->LocateHandleBuffer(
        ByProtocol,
        &gEfiSimpleNetworkProtocolGuid,
        NULL,
        &handle_count,
        &handles
    );
    
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to locate network handles: %r\r\n", status);
        return status;
    }
    
    if (handle_count == 0) {
        Print(L"No network interfaces found\r\n");
        return EFI_NOT_FOUND;
    }
    
    Print(L"Found %u network interface(s)\r\n", handle_count);
    
    // Initialize each interface
    for (i = 0; i < handle_count && i < 16; i++) {
        EFI_SIMPLE_NETWORK_PROTOCOL *simple_network;
        
        status = BS->HandleProtocol(
            handles[i],
            &gEfiSimpleNetworkProtocolGuid,
            (VOID **)&simple_network
        );
        
        if (EFI_ERROR(status)) {
            continue;
        }
        
        g_network_interfaces[g_network_interface_count].handle = handles[i];
        g_network_interfaces[g_network_interface_count].simple_network = simple_network;
        g_network_interfaces[g_network_interface_count].initialized = 0;
        g_network_interface_count++;
        
        Print(L"  Interface %u: %p\r\n", i, handles[i]);
    }
    
    // Try to get PXE Base Code protocol
    for (i = 0; i < g_network_interface_count; i++) {
        status = BS->HandleProtocol(
            g_network_interfaces[i].handle,
            &gEfiPxeBaseCodeProtocolGuid,
            (VOID **)&g_network_interfaces[i].pxe
        );
        
        if (!EFI_ERROR(status)) {
            Print(L"  PXE Base Code protocol found on interface %u\r\n", i);
        }
    }
    
    // Try to get Load File protocols
    for (i = 0; i < g_network_interface_count; i++) {
        status = BS->HandleProtocol(
            g_network_interfaces[i].handle,
            &gEfiLoadFileProtocolGuid,
            (VOID **)&g_network_interfaces[i].load_file
        );
        
        if (!EFI_ERROR(status)) {
            Print(L"  Load File protocol found on interface %u\r\n", i);
        }
        
        status = BS->HandleProtocol(
            g_network_interfaces[i].handle,
            &gEfiLoadFile2ProtocolGuid,
            (VOID **)&g_network_interfaces[i].load_file2
        );
        
        if (!EFI_ERROR(status)) {
            Print(L"  Load File2 protocol found on interface %u\r\n", i);
        }
    }
    
    if (handles) {
        BS->FreePool(handles);
    }
    
    return EFI_SUCCESS;
}

//
// DHCP Configuration
//

EFI_STATUS
network_configure_dhcp(network_interface_t *netif)
{
    EFI_STATUS status;
    EFI_PXE_BASE_CODE_PROTOCOL *pxe;
    EFI_PXE_BASE_CODE_MODE *mode;
    
    if (!netif || !netif->pxe) {
        return EFI_UNSUPPORTED;
    }
    
    pxe = netif->pxe;
    
    Print(L"Configuring DHCP...\r\n");
    
    // Get current mode
    status = pxe->GetMode(pxe, &mode, NULL);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get PXE mode: %r\r\n", status);
        return status;
    }
    
    // Check if already configured
    if (mode->Started) {
        Print(L"PXE already started\r\n");
        
        // Get DHCP info
        if (mode->DhcpAckReceived) {
            Print(L"DHCP ACK received\r\n");
            netif->ip_addr = mode->StationIp.v4.Addr;
            netif->subnet_mask = mode->SubnetMask.v4.Addr;
            netif->gateway = mode->GatewayIp.v4.Addr;
            netif->dns = mode->DnsIp.v4.Addr;
            netif->server_ip = mode->ServerIp.v4.Addr;
            
            // Copy boot file name
            if (mode->BootFileName[0] != 0) {
                StrnCpy(g_dhcp_bootfile, mode->BootFileName, 256);
            }
            
            g_dhcp_discovered = 1;
            return EFI_SUCCESS;
        }
    }
    
    // Start PXE
    Print(L"Starting PXE...\r\n");
    
    // Use default settings for now
    EFI_PXE_BASE_CODE_MODE new_mode = {
        .Started = FALSE,
        .Ipv6Available = FALSE,
        .Ipv6Supported = FALSE,
        .UsingIpv6 = FALSE,
        .BisSupported = FALSE,
        .BisDetected = FALSE,
        .AutoArp = TRUE,
        .SendGratArp = TRUE,
        .DhcpDiscovered = FALSE,
        .DhcpAckReceived = FALSE,
        .ProxyOfferReceived = FALSE,
        .PxeDiscoverValid = FALSE,
        .PxeReplyReceived = FALSE,
        .PxeBisReplyReceived = FALSE,
        .IcmpErrorReceived = FALSE,
        .TftpErrorReceived = FALSE,
        .MakeCallback = FALSE,
        .KeepAlive = FALSE
    };
    
    status = pxe->Start(pxe, &new_mode);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to start PXE: %r\r\n", status);
        return status;
    }
    
    // Wait for DHCP
    Print(L"Waiting for DHCP...\r\n");
    
    for (UINTN i = 0; i < NETWORK_BOOT_TIMEOUT; i++) {
        status = pxe->GetMode(pxe, &mode, NULL);
        if (EFI_ERROR(status)) {
            continue;
        }
        
        if (mode->DhcpAckReceived) {
            Print(L"DHCP ACK received\r\n");
            netif->ip_addr = mode->StationIp.v4.Addr;
            netif->subnet_mask = mode->SubnetMask.v4.Addr;
            netif->gateway = mode->GatewayIp.v4.Addr;
            netif->dns = mode->DnsIp.v4.Addr;
            netif->server_ip = mode->ServerIp.v4.Addr;
            
            if (mode->BootFileName[0] != 0) {
                StrnCpy(g_dhcp_bootfile, mode->BootFileName, 256);
            }
            
            g_dhcp_discovered = 1;
            return EFI_SUCCESS;
        }
        
        // Wait 1 second
        BS->Stall(1000000);
    }
    
    Print(L"ERROR: DHCP timeout\r\n");
    return EFI_TIMEOUT;
}

//
// File Loading
//

EFI_STATUS
network_load_file(network_interface_t *netif, CHAR16 *filename, VOID **buffer, UINTN *size)
{
    EFI_STATUS status;
    EFI_LOAD_FILE_PROTOCOL *load_file;
    EFI_DEVICE_PATH_PROTOCOL *device_path;
    VOID *file_buffer = NULL;
    UINTN file_size = 0;
    
    if (!netif || !filename || !buffer || !size) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Try LoadFile2 first
    if (netif->load_file2) {
        Print(L"Trying LoadFile2 protocol...\r\n");
        
        status = netif->load_file2->LoadFile(
            netif->load_file2,
            filename,
            FALSE,
            &file_size,
            &file_buffer
        );
        
        if (!EFI_ERROR(status)) {
            *buffer = file_buffer;
            *size = file_size;
            Print(L"File loaded via LoadFile2: %u bytes\r\n", file_size);
            return EFI_SUCCESS;
        }
        
        Print(L"LoadFile2 failed: %r\r\n", status);
    }
    
    // Try LoadFile
    if (netif->load_file) {
        Print(L"Trying LoadFile protocol...\r\n");
        
        // Create device path for the file
        // For now, we'll use a simple approach
        status = netif->load_file->LoadFile(
            netif->load_file,
            filename,
            FALSE,
            &file_size,
            &file_buffer
        );
        
        if (!EFI_ERROR(status)) {
            *buffer = file_buffer;
            *size = file_size;
            Print(L"File loaded via LoadFile: %u bytes\r\n", file_size);
            return EFI_SUCCESS;
        }
        
        Print(L"LoadFile failed: %r\r\n", status);
    }
    
    // Try PXE Base Code
    if (netif->pxe) {
        Print(L"Trying PXE Base Code protocol...\r\n");
        
        // Use PXE to download file
        // This is a simplified approach
        status = pxe_download_file(netif, filename, buffer, size);
        if (!EFI_ERROR(status)) {
            return EFI_SUCCESS;
        }
    }
    
    return EFI_UNSUPPORTED;
}

//
// PXE File Download (Simplified TFTP)
//

EFI_STATUS
pxe_download_file(network_interface_t *netif, CHAR16 *filename, VOID **buffer, UINTN *size)
{
    EFI_STATUS status;
    EFI_PXE_BASE_CODE_PROTOCOL *pxe;
    EFI_PXE_BASE_CODE_TFTP_OPCODE tftp_opcode;
    VOID *tftp_buffer = NULL;
    UINTN tftp_size = 0;
    UINT64 tftp_block_size = TFTP_BLOCK_SIZE;
    
    if (!netif || !netif->pxe || !filename || !buffer || !size) {
        return EFI_INVALID_PARAMETER;
    }
    
    pxe = netif->pxe;
    
    Print(L"Downloading %s via TFTP...\r\n", filename);
    
    // Use PXE TFTP read
    status = pxe->TftpReadFile(
        pxe,
        filename,
        NULL,  // Use default server
        &tftp_opcode,
        &tftp_block_size,
        &tftp_size,
        &tftp_buffer
    );
    
    if (EFI_ERROR(status)) {
        Print(L"ERROR: TFTP download failed: %r\r\n", status);
        return status;
    }
    
    Print(L"TFTP download successful: %u bytes\r\n", tftp_size);
    
    *buffer = tftp_buffer;
    *size = tftp_size;
    
    return EFI_SUCCESS;
}

//
// Kernel Download
//

EFI_STATUS
network_download_kernel(VOID **kernel_buffer, UINTN *kernel_size)
{
    EFI_STATUS status;
    CHAR16 *kernel_filename;
    
    if (!kernel_buffer || !kernel_size) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Try DHCP bootfile first
    if (g_dhcp_discovered && g_dhcp_bootfile[0] != 0) {
        kernel_filename = g_dhcp_bootfile;
        Print(L"Using DHCP bootfile: %s\r\n", kernel_filename);
    } else {
        kernel_filename = L"otsos/kernel.elf";
        Print(L"Using default bootfile: %s\r\n", kernel_filename);
    }
    
    // Download kernel
    status = network_load_file(g_selected_interface, kernel_filename, kernel_buffer, kernel_size);
    
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to download kernel: %r\r\n", status);
        return status;
    }
    
    Print(L"Kernel downloaded: %u bytes\r\n", *kernel_size);
    return EFI_SUCCESS;
}

//
// Config Download
//

EFI_STATUS
network_download_config(VOID **config_buffer, UINTN *config_size)
{
    EFI_STATUS status;
    CHAR16 *config_filename = L"otsos/config.toml";
    
    if (!config_buffer || !config_size) {
        return EFI_INVALID_PARAMETER;
    }
    
    // Download config
    status = network_load_file(g_selected_interface, config_filename, config_buffer, config_size);
    
    if (EFI_ERROR(status)) {
        Print(L"WARNING: Failed to download config: %r\r\n", status);
        *config_buffer = NULL;
        *config_size = 0;
        return EFI_SUCCESS;  // Config is optional
    }
    
    Print(L"Config downloaded: %u bytes\r\n", *config_size);
    return EFI_SUCCESS;
}

//
// Utility Functions
//

EFI_STATUS
network_get_mac_address(network_interface_t *netif)
{
    EFI_STATUS status;
    EFI_SIMPLE_NETWORK_PROTOCOL *simple_network;
    EFI_MAC_ADDRESS mac_addr;
    
    if (!netif) {
        return EFI_INVALID_PARAMETER;
    }
    
    simple_network = netif->simple_network;
    
    // Get MAC address
    status = simple_network->GetMacAddress(simple_network, &mac_addr);
    if (EFI_ERROR(status)) {
        Print(L"ERROR: Failed to get MAC address: %r\r\n", status);
        return status;
    }
    
    // Copy MAC address
    netif->mac_addr_len = mac_addr.AddrLen;
    MemCpy(netif->mac_addr, mac_addr.Addr, mac_addr.AddrLen);
    
    Print(L"MAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
        netif->mac_addr[0], netif->mac_addr[1], netif->mac_addr[2],
        netif->mac_addr[3], netif->mac_addr[4], netif->mac_addr[5]);
    
    return EFI_SUCCESS;
}

EFI_STATUS
network_get_ip_config(network_interface_t *netif)
{
    if (!netif) {
        return EFI_INVALID_PARAMETER;
    }
    
    // For now, just return the DHCP info
    // In a full implementation, we would get this from the network protocol
    
    if (netif->use_dhcp) {
        Print(L"IP: %d.%d.%d.%d\r\n",
            (netif->ip_addr >> 24) & 0xFF,
            (netif->ip_addr >> 16) & 0xFF,
            (netif->ip_addr >> 8) & 0xFF,
            netif->ip_addr & 0xFF);
        
        Print(L"Mask: %d.%d.%d.%d\r\n",
            (netif->subnet_mask >> 24) & 0xFF,
            (netif->subnet_mask >> 16) & 0xFF,
            (netif->subnet_mask >> 8) & 0xFF,
            netif->subnet_mask & 0xFF);
        
        Print(L"Gateway: %d.%d.%d.%d\r\n",
            (netif->gateway >> 24) & 0xFF,
            (netif->gateway >> 16) & 0xFF,
            (netif->gateway >> 8) & 0xFF,
            netif->gateway & 0xFF);
    }
    
    return EFI_SUCCESS;
}

VOID
network_dump_info(network_interface_t *netif)
{
    if (!netif) {
        return;
    }
    
    Print(L"\r\nNetwork Interface Info:\r\n");
    Print(L"  Handle: %p\r\n", netif->handle);
    Print(L"  MAC: %02X:%02X:%02X:%02X:%02X:%02X\r\n",
        netif->mac_addr[0], netif->mac_addr[1], netif->mac_addr[2],
        netif->mac_addr[3], netif->mac_addr[4], netif->mac_addr[5]);
    
    if (netif->use_dhcp) {
        Print(L"  DHCP: Yes\r\n");
        Print(L"  IP: %d.%d.%d.%d\r\n",
            (netif->ip_addr >> 24) & 0xFF,
            (netif->ip_addr >> 16) & 0xFF,
            (netif->ip_addr >> 8) & 0xFF,
            netif->ip_addr & 0xFF);
        Print(L"  Server: %d.%d.%d.%d\r\n",
            (netif->server_ip >> 24) & 0xFF,
            (netif->server_ip >> 16) & 0xFF,
            (netif->server_ip >> 8) & 0xFF,
            netif->server_ip & 0xFF);
    } else {
        Print(L"  DHCP: No\r\n");
    }
    
    Print(L"\r\n");
}
