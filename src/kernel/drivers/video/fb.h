#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <kernel/multiboot.h>
#include <kernel/multiboot2.h>
#include <mlibc/mlibc.h>

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
