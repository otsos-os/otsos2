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

## 4. VGA & Framebuffer Driver (`src/kernel/drivers/vga.c`, `src/kernel/drivers/video/fb.c`)
Unified driver handling both legacy text mode and high-resolution VBE framebuffer.
- **Modes**: 
    - Legacy: 80x25 Text Mode (0xB8000).
    - Graphical: High-resolution (e.g., 1024x768x32) linear framebuffer.
- **Features**:
    - **Dynamic Mode Switching**: Automatically uses framebuffer if initialized.
    - **Font Rendering**: Uses embedded Spleen font (8x16) for text on graphics.
    - **Text Output**: Standard `printf` works in both modes.
    - **ANSI Parsing**: Supports ANSI escape codes for colored text (e.g., `\033[31m`).
        - Colors: 30-37 (Black, Red, Green, Yellow, Blue, Magenta, Cyan, White).
    - **Scrolling**: Hardware-accelerated scrolling in both text and graphics modes.
- **API**:
    - `void printf(const char *fmt, ...)`: Universal formatted output.
    - `void clear_scr()`: Clears screen (black).
    - `void fb_init(multiboot_info_t *mb_info)`: Initializes VBE framebuffer.
    - `void fb_put_pixel(int x, int y, u32 color)`: Draw single pixel.
    - `void fb_put_char(int x, int y, char c, u32 color)`: Draw character glyph.
    - `void vga_set_color(u8 color)`: Set text color (works in Text & FB modes).

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

