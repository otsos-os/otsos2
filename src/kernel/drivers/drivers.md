# Drivers

OTSOS supports several core drivers for hardware interaction, storage, and user input.

## 1. Storage: PATA Driver (`src/kernel/drivers/disk/pata/pata.c`)
Driver for IDE/PATA hard drives using PIO mode.
- **Support**: Identify drive, read blocks, and write blocks.

## 2. File System: ChainFS (`src/kernel/drivers/fs/chainFS/chainfs.c`)
The default file system for OTSOS.
- **Type**: Linked-block based file system.
- **Features**:
    - File creation, deletion, reading, and writing.
    - Basic directory support.

## 3. Serial Port (COM1) (`src/lib/com1.c`)
Used primarily for kernel debugging and logging.
- **Port**: `0x3F8`.

## 4. VGA Driver (`src/kernel/drivers/vga.c`)
Basic text mode driver for visual output (0xB8000).
- **Mode**: 80x25 Text Mode.
- **Features**:
    - Text output with color support.
    - Automatic scrolling.
    - Standard `printf` implementation.
- **API**:
    - `void printf(const char *fmt, ...)`: Formatted output to the screen.
    - `void clear_scr()`: Clear the terminal.
    - `void scroll_scr()`: Scroll the screen one line up.
    - `void vga_set_color(u8 color)`: Set current text attributes.

## 5. Keyboard Manager (`src/kernel/drivers/keyboard/keyboard.c`)
Global keyboard abstraction layer for managing input drivers.
- **Features**:
    - Dynamic driver detection and switching.
    - Common interface for `getchar` and interrupt handling.
    - Serial logging of driver status.
- **API**:
    - `void keyboard_manager_init()`: Detect and initialize available keyboard drivers.
    - `char keyboard_getchar()`: Read a character from the active driver's buffer.

## 6. PS/2 Keyboard Driver (`src/kernel/drivers/keyboard/ps2.c`)
Driver for standard PS/2 keyboards.
- **Ports**: Control `0x64`, Data `0x60`.
- **Features**:
    - Scancode Set 1 decoding (US Layout).
    - Modifier key support (Shift, Caps Lock).
    - Circular input buffer.
