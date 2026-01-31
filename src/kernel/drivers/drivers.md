# Drivers

OTSOS supports several core drivers for hardware interaction, storage, and user input.

## 1. Storage: PATA Driver (`src/kernel/drivers/disk/pata/pata.c`)
Driver for IDE/PATA hard drives using PIO mode.
- **Support**: Identify drive, read blocks, and write blocks.
- **API**:
    - `void pata_read_block(u32 lba, u8 *buffer)`
    - `void pata_write_block(u32 lba, u8 *buffer)`

## 2. File System: ChainFS (`src/kernel/drivers/fs/chainFS/chainfs.c`)
The default file system for OTSOS.
- **Type**: Linked-block based file system.
- **Features**:
    - File creation, deletion, reading, and writing.
    - Basic directory support.
- **API**:
    - `int fs_read_file(const char *path, char *buffer)`
    - `int fs_write_file(const char *path, const char *data, int size)`

## 3. Serial Port (COM1) (`src/lib/com1.c`)
Used primarily for kernel debugging and logging.
- **Port**: `0x3F8`.
- **API**:
    - `void com1_printf(const char *format, ...)`: Formatted output to the serial console.

