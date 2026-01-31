#ifndef VGA_H
#define VGA_H

#include <mlibc/mlibc.h>

void clear_scr();
void scroll_scr();
void printf(const char *fmt, ...);
void vga_putc(char c);
void vga_puts(const char *str);

#endif
