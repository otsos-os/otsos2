/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/drivers/video/fb.h>
#include <lib/com1.h>

static u32 *framebuffer = 0;
static u32 pitch = 0;
static u32 width = 0;
static u32 height = 0;
static u8 bpp = 0;

void fb_init_mb2(multiboot2_info_t *mb_info) {
  multiboot2_tag_framebuffer_t *fb_tag =
      (multiboot2_tag_framebuffer_t *)multiboot2_find_tag(
          mb_info, MULTIBOOT2_TAG_TYPE_FRAMEBUFFER);

  if (!fb_tag) {
    com1_write_string("[FB] Framebuffer tag not found in Multiboot2 info!\n");
    return;
  }

  framebuffer = (u32 *)(u64)fb_tag->framebuffer_addr;
  pitch = fb_tag->framebuffer_pitch;
  width = fb_tag->framebuffer_width;
  height = fb_tag->framebuffer_height;
  bpp = fb_tag->framebuffer_bpp;

  com1_write_string("[FB] Multiboot2 Initialized:\n");
  com1_write_string("  Addr: 0x");
  com1_write_hex_qword((u64)framebuffer);
  com1_newline();
  com1_write_string("  Res: ");
  com1_write_dec(width);
  com1_write_string("x");
  com1_write_dec(height);
  com1_newline();
  com1_write_string("  BPP: ");
  com1_write_dec(bpp);
  com1_newline();
  com1_write_string("  Type: ");
  com1_write_dec(fb_tag->framebuffer_type);
  com1_newline();

  u64 fb_addr = (u64)framebuffer;
  u64 fb_size = (u64)pitch * height;
#define PAGE_SIZE 4096
  u64 fb_pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

  extern void mmu_map_page(u64 vaddr, u64 paddr, u64 flags);

  for (u64 i = 0; i < fb_pages; i++) {
    mmu_map_page(fb_addr + i * PAGE_SIZE, fb_addr + i * PAGE_SIZE, 0x3);
  }

  fb_clear(0x0000FF);
}

/* Legacy Multiboot1 init - kept for compatibility */
void fb_init(multiboot_info_t *mb_info) {
  if ((mb_info->flags & (1 << 12)) == 0) {
    com1_write_string("[FB] Framebuffer flag not set in multiboot header!\n");
    return;
  }

  framebuffer = (u32 *)(u64)mb_info->framebuffer_addr;
  pitch = mb_info->framebuffer_pitch;
  width = mb_info->framebuffer_width;
  height = mb_info->framebuffer_height;
  bpp = mb_info->framebuffer_bpp;

  com1_write_string("[FB] Initialized:\n");
  com1_write_string("  Addr: 0x");
  com1_write_hex_qword((u64)framebuffer);
  com1_newline();
  com1_write_string("  Res: ");
  com1_write_dec(width);
  com1_write_string("x");
  com1_write_dec(height);
  com1_newline();
  com1_write_string("  BPP: ");
  com1_write_dec(bpp);
  com1_newline();

  fb_clear(0x0000FF);
}

void fb_put_pixel(int x, int y, u32 color) {
  if (!framebuffer)
    return;
  if (x < 0 || x >= (int)width || y < 0 || y >= (int)height)
    return;

  u64 offset = y * pitch + x * (bpp / 8);
  *(volatile u32 *)((u8 *)framebuffer + offset) = color;
}

void fb_clear(u32 color) {
  if (!framebuffer)
    return;
  for (int y = 0; y < (int)height; y++) {
    for (int x = 0; x < (int)width; x++) {
      fb_put_pixel(x, y, color);
    }
  }
}

extern const u8 glyph_32_data[];

extern const u8 *get_font_data(char c);

void fb_put_char(int x, int y, char c, u32 color) {
  const u8 *data = get_font_data(c);
  if (!data)
    return;

  u32 bg_color = 0x000000; // Assuming black background for now

  for (int row = 0; row < 16; row++) {
    u8 bitmap_row = data[row];
    for (int col = 0; col < 8; col++) {
      if (bitmap_row & (1 << (7 - col))) {
        fb_put_pixel(x + col, y + row, color);
      } else {
        fb_put_pixel(x + col, y + row, bg_color);
      }
    }
  }
}

void fb_write_string(int x, int y, const char *str, u32 color) {
  int cur_x = x;
  int cur_y = y;
  while (*str) {
    if (*str == '\n') {
      cur_x = x;
      cur_y += 16;
    } else {
      fb_put_char(cur_x, cur_y, *str, color);
      cur_x += 8;
    }
    str++;
  }
}

int is_framebuffer_enabled() { return framebuffer != 0; }

u32 fb_get_width() { return width; }
u32 fb_get_height() { return height; }

void fb_scroll(int lines) {
  if (!framebuffer)
    return;

  u32 line_height = 16;
  u32 lines_to_scroll = lines * line_height;
  u32 bytes_per_line = pitch;
  u32 total_lines = height;

  if (lines_to_scroll >= total_lines) {
    fb_clear(0x000000);
    return;
  }

  u32 bytes_to_copy = bytes_per_line * (total_lines - lines_to_scroll);
  u32 *dst = framebuffer;
  u32 *src = (u32 *)((u8 *)framebuffer + (bytes_per_line * lines_to_scroll));

  u8 *fb_bytes = (u8 *)framebuffer;
  u64 i;
  for (i = 0; i < bytes_to_copy; i++) {
    fb_bytes[i] = fb_bytes[i + (bytes_per_line * lines_to_scroll)];
  }

  u32 bytes_to_clear = bytes_per_line * lines_to_scroll;
  u8 *bottom_start = fb_bytes + bytes_to_copy;
  for (i = 0; i < bytes_to_clear; i++) {
    bottom_start[i] = 0;
  }
}
