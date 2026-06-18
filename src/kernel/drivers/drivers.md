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

## 4. Video architecture: GPU drivers under DRM

OTSOS targets modern (UEFI) systems and ships no legacy VGA text-mode
driver. All display output goes through the DRM subsystem, which manages a
stack of GPU backend drivers plus a frame/monitor pipeline.

Layering (top to bottom):

    userspace (drmCall syscall) / tty / panic / kshell / console
                        |
                  DRM frontend  (drawing: put_pixel, fill_rect,
                                 put_char_cell, clear, scroll, batch)
                        |
                  DRM atomic core (frame, shadow buffer, dirty
                                   tracking, monitor/connector state)
                        |
                  DRM driver manager (selects a GPU driver)
                        |
                  GPU backend drivers (hardware only)

- `kernel/console.c` / `console.h`: kernel console layer. Provides
  `printf`, `clear_scr`, `console_putchar`, `console_put_entry_at`,
  `console_get_width/height`. Routes to the active tty when ready, else
  draws straight to the DRM frontend with an internal cursor (early boot).
- `kernel/drivers/video/fb.c` / `fb.h`: the **fbdev GPU backend driver**.
  Probes the boot display, maps the linear framebuffer, and implements
  `present` / `present_rect` to blit a finished frame to the hardware. It
  does NO drawing — rendering happens in the DRM frontend.
- `kernel/drivers/video/drm/`:
    - `driver.h` / `driver.c`: `drm_driver_t` model and driver selection
      (auto or preferred name). The fbdev driver is registered here.
    - `atomic.c` / `atomic.h`: atomic state machine and commit path
      (CRTC, primary plane, connector/monitor).
    - `frontend.c` / `frontend.h`: drawing API used by tty/kshell/panic
      and the `drmCall` syscall.
    - `init.c` / `init.h`: multiboot framebuffer probing and DRM bring-up.
    - `types.h`: shared DRM types.
- The old `vga.c`/`vga.h` and `drm/backend.c`/`backend.h` were removed.

## 6. Keyboard Manager (`src/kernel/drivers/keyboard/keyboard.c`)
Global keyboard abstraction layer for managing input drivers.
- **Features**:
    - Dynamic driver detection and switching.
    - Common interface for `getchar` and interrupt handling.
    - Serial logging of driver status.
- **API**:
    - `void keyboard_manager_init()`: Detect and initialize available keyboard drivers.
    - `char keyboard_getchar()`: Read a character from the active driver's buffer.

## 7. PS/2 Keyboard Driver (`src/kernel/drivers/keyboard/ps2.c`)
Driver for standard PS/2 keyboards.
- **Ports**: Control `0x64`, Data `0x60`.
- **Features**:
    - Scancode Set 1 decoding (US Layout).
    - Modifier key support (Shift, Caps Lock).
    - Circular input buffer.

## 8. PIT Timer Driver (`src/kernel/drivers/timer.c`)
interval timer PIT for system ticks.
- **ports**: `0x40` , `0x43`
- **feature**:
    - soon нахуй
- **API**:
    - `void timer_init(u32 frequency)`: set frequency
    - `u64 timer_get_ticks()`: return number of tick since boot

## 9. TTY (`src/kernel/drivers/tty.c`)
Console manager joining keyboard input with DRM frontend output.
- userspace API: termRead, termWrite
- switch: Ctrl + Numpad 0..9
