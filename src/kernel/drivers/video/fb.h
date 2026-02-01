#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <kernel/multiboot2.h>
#include <mlibc/mlibc.h>

/* Legacy Multiboot1 structure - kept for reference, but not used with
 * Multiboot2 */
typedef struct {
  u32 flags;
  u32 mem_lower;
  u32 mem_upper;
  u32 boot_device;
  u32 cmdline;
  u32 mods_count;
  u32 mods_addr;
  u32 syms[4];
  u32 mmap_length;
  u32 mmap_addr;
  u32 drives_length;
  u32 drives_addr;
  u32 config_table;
  u32 boot_loader_name;
  u32 apm_table;
  u32 vbe_control_info;
  u32 vbe_mode_info;
  u16 vbe_mode;
  u16 vbe_interface_seg;
  u16 vbe_interface_off;
  u16 vbe_interface_len;

  // Framebuffer fields (if bit 12 of flags is set)
  u64 framebuffer_addr;
  u32 framebuffer_pitch;
  u32 framebuffer_width;
  u32 framebuffer_height;
  u8 framebuffer_bpp;
  u8 framebuffer_type;
  u8 color_info[6];
} __attribute__((packed)) multiboot_info_t;

void fb_init_mb2(multiboot2_info_t *mb_info);

void fb_init(multiboot_info_t *mb_info);

void fb_put_pixel(int x, int y, u32 color);
void fb_clear(u32 color);
int is_framebuffer_enabled();

void fb_put_char(int x, int y, char c, u32 color);
void fb_write_string(int x, int y, const char *str, u32 color);
u32 fb_get_width();
u32 fb_get_height();
void fb_scroll(int lines);

#endif
