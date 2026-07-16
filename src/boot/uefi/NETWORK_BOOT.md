# OTSOS UEFI Network Boot (PXE) Guide

## 🌐 Overview

This document describes the **UEFI Network Boot (PXE)** functionality added to the OTSOS UEFI bootloader. This allows OTSOS to be loaded and booted over the network using standard UEFI PXE protocols.

---

## 🎯 Features

### ✅ **Implemented**

1. **UEFI Network Protocol Support**
   - Simple Network Protocol detection
   - PXE Base Code Protocol support
   - Load File Protocol support (v1 and v2)

2. **DHCP Support**
   - Automatic IP configuration via DHCP
   - PXE Discover and ACK handling
   - Boot file name from DHCP options

3. **File Download**
   - TFTP file download via PXE
   - LoadFile protocol support
   - Multiple protocol fallback

4. **Boot Source Detection**
   - Automatic network boot attempt
   - Fallback to local storage
   - Boot source flag in kernel info

### 📋 **Boot Process**

```
UEFI Firmware
    ↓
uefi_loader.efi
    ↓
1. Initialize UEFI environment
2. Discover network interfaces
3. Attempt DHCP configuration
4. Try network kernel download
   ↓ [If successful]
   Download kernel via TFTP/PXE
   ↓
   Jump to kernel
   
   ↓ [If failed]
5. Fall back to local storage
6. Load kernel from disk
7. Jump to kernel
```

---

## 🔧 Configuration

### 1. **DHCP Server Configuration**

To enable network boot, configure your DHCP server to provide PXE boot information:

```
# DHCP Configuration Example (ISC DHCP)
subnet 192.168.1.0 netmask 255.255.255.0 {
    range 192.168.1.100 192.168.1.200;
    option routers 192.168.1.1;
    option domain-name-servers 192.168.1.1;
    
    # PXE Boot Options
    if exists user-class and option user-class = "iPXE" {
        filename "otsos/kernel.elf";
    } else {
        filename "otsos/kernel.elf";
    }
    
    # TFTP Server
    option tftp-server-name "192.168.1.10";
    
    # Boot file (optional, overrides filename)
    option bootfile-name "otsos/kernel.elf";
}
```

### 2. **TFTP Server Configuration**

Set up a TFTP server with the following files:

```
/tftpboot/
├── otsos/
│   ├── kernel.elf          # OTSOS kernel
│   └── config.toml        # Optional configuration
```

### 3. **PXE Boot Menu (Optional)**

For advanced PXE setups, you can use iPXE or similar:

```
# iPXE Configuration
:otsos
    kernel otsos/kernel.elf
    initrd otsos/initrd.img
    boot
```

---

## 🚀 Usage

### 1. **Enable Network Boot in UEFI**

Most UEFI systems have network boot disabled by default. Enable it in your firmware settings:

1. Enter UEFI Setup (usually F2, F12, DEL, or ESC during boot)
2. Find "Boot Options" or "Boot Order"
3. Enable "Network Boot" or "PXE Boot"
4. Move network boot to the top of the boot order
5. Save and exit

### 2. **Boot from Network**

When the system boots:

1. UEFI firmware initializes network interface
2. DHCP request is sent
3. PXE bootloader (uefi_loader.efi) is loaded from TFTP
4. Kernel is downloaded from TFTP
5. OTSOS boots normally

### 3. **Fallback to Local Boot**

If network boot fails:
- The bootloader automatically falls back to local storage
- Tries to load kernel from `/EFI/otsos/kernel.elf`
- Continues normal boot process

---

## 📊 Implementation Details

### Network Interface Discovery

The bootloader uses UEFI protocols to discover network interfaces:

```c
// Locate all handles with Simple Network Protocol
status = BS->LocateHandleBuffer(
    ByProtocol,
    &gEfiSimpleNetworkProtocolGuid,
    NULL,
    &handle_count,
    &handles
);
```

### DHCP Configuration

Using PXE Base Code Protocol:

```c
// Start PXE
status = pxe->Start(pxe, &new_mode);

// Wait for DHCP ACK
for (i = 0; i < NETWORK_BOOT_TIMEOUT; i++) {
    pxe->GetMode(pxe, &mode, NULL);
    if (mode->DhcpAckReceived) {
        // DHCP successful
        netif->ip_addr = mode->StationIp.v4.Addr;
        netif->server_ip = mode->ServerIp.v4.Addr;
        break;
    }
    BS->Stall(1000000);  // Wait 1 second
}
```

### File Download

Multiple methods are tried in order:

1. **LoadFile2 Protocol** (preferred)
2. **LoadFile Protocol**
3. **PXE TFTP Download**

```c
// Try LoadFile2 first
if (netif->load_file2) {
    status = netif->load_file2->LoadFile(
        netif->load_file2,
        filename,
        FALSE,
        &file_size,
        &file_buffer
    );
}

// Fall back to PXE TFTP
if (EFI_ERROR(status) && netif->pxe) {
    status = pxe->TftpReadFile(
        pxe,
        filename,
        NULL,
        &tftp_opcode,
        &tftp_block_size,
        &tftp_size,
        &tftp_buffer
    );
}
```

---

## 🔌 Kernel Integration

### Boot Flag Detection

The kernel can detect network boot via the `boot_flags` field:

```c
// In kernel.c
if (magic == UEFI_BOOTLOADER_MAGIC) {
    uefi_info = (uefi_boot_info_t *)addr;
    
    if (uefi_info->boot_flags & BOOT_FLAG_NETWORK) {
        printk("Network boot detected\n");
        // Network-specific initialization
    }
}
```

### Boot Source Information

The `uefi_boot_info_t` structure contains:

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
    u64 kernel_physical_addr;
    u64 kernel_size;
    u64 boot_option;
    u64 boot_flags;  // BOOT_FLAG_NETWORK set if network boot
} uefi_boot_info_t;
```

---

## 🛠 Customization

### 1. **Change Default Boot File**

Edit `network.c`:

```c
// Change this line
#define DEFAULT_BOOTFILE "otsos/kernel.elf"
```

### 2. **Add Custom DHCP Options**

Extend the DHCP parsing in `network_configure_dhcp()`:

```c
// Add support for custom options
if (mode->PxeDiscoverValid) {
    // Parse PXE discover packet for custom options
}
```

### 3. **Add HTTP Boot Support**

For modern networks, you can add HTTP boot support:

```c
// Add to network.c
EFI_STATUS
http_download_file(CHAR16 *url, VOID **buffer, UINTN *size)
{
    // Implement HTTP download using UEFI HTTP protocols
}
```

---

## 🔍 Troubleshooting

### Common Issues

| Issue | Solution |
|-------|----------|
| **No network interfaces found** | Check if network card has UEFI support |
| **DHCP timeout** | Verify DHCP server is running and reachable |
| **TFTP download failed** | Check TFTP server is running and file exists |
| **PXE not in boot order** | Enable PXE boot in UEFI settings |
| **Wrong boot file** | Check DHCP option 67 or filename setting |

### Debug Output

The bootloader provides debug output via UEFI console:

```
OTSOS UEFI Bootloader
Initializing...
Trying network boot...
Discovering network interfaces...
Found 1 network interface(s)
  Interface 0: 0x12345678
MAC address: 00:11:22:33:44:55
Configuring DHCP...
Starting PXE...
Waiting for DHCP...
DHCP ACK received
IP: 192.168.1.100
Network boot available
Memory map obtained
Attempting network kernel download...
Downloading otsos/kernel.elf via TFTP...
TFTP download successful: 1234567 bytes
Kernel downloaded via network: 1234567 bytes
Ready to jump to kernel...
```

### Enable Serial Debugging

For more detailed debugging, enable serial output:

```c
// In efi_main()
// Initialize serial port
EFI_STATUS init_serial()
{
    // Configure serial port for debugging
}
```

---

## 📚 Protocol References

### UEFI Network Protocols

1. **EFI_SIMPLE_NETWORK_PROTOCOL**
   - Basic network operations
   - MAC address access
   - Packet transmission/reception

2. **EFI_PXE_BASE_CODE_PROTOCOL**
   - PXE boot support
   - DHCP configuration
   - TFTP download

3. **EFI_LOAD_FILE_PROTOCOL**
   - File download over network
   - TFTP and HTTP support

4. **EFI_LOAD_FILE2_PROTOCOL**
   - Enhanced file download
   - Better error handling

### DHCP Options

| Option | Description | Value |
|--------|-------------|-------|
| 66 | TFTP Server | IP address |
| 67 | Boot File | String |
| 17 | Root Path | String |

---

## 🎯 Future Enhancements

### 1. **HTTP Boot**
- Support for HTTP file downloads
- HTTPS support for secure boot

### 2. **iPXE Compatibility**
- Full iPXE script support
- Advanced boot menus

### 3. **Multicast Boot**
- Support for multicast TFTP
- Simultaneous deployment to multiple machines

### 4. **Network Configuration**
- Static IP configuration
- VLAN support
- Proxy DHCP

### 5. **Security**
- Secure Boot support
- TLS for HTTP downloads
- File verification

---

## 📄 License

All network boot code is licensed under **BSD 2-Clause License**, consistent with OTSOS.

---

**Status:** ✅ **IMPLEMENTED**  
**Version:** 1.0  
**Date:** July 2026  
**Compatibility:** UEFI 2.x, x86_64
