# MLIBC - Minimalistic C Library

MLIBC is a minimalistic implementation of the standard C library, tailored specifically for the OTSOS kernel environment. It provides essential functions for string manipulation, memory management, I/O operations, and low-level hardware interaction.

## Modules

### 1. Memory Management (`memory.h`)
Core kernel heap management.
- **API**: `kmalloc`, `kfree`, `kcalloc`, `krealloc`, `kmalloc_aligned`.
- **Details**: See [Memory Management](memory.md).

### 2. String & Memory Operations (`mlibc.h` / `string.c`)
Basic functions for manipulating strings and raw memory blocks.
- `int strlen(const char *str)`: Returns the length of a string.
- `int strcmp(const char *str1, const char *str2)`: Compares two strings.
- `char *strcpy(char *dest, const char *src)`: Copies a string to another buffer.
- `char *strcat(char *dest, const char *src)`: Concatenates two strings.
- `char *strchr(const char *str, int c)`: Finds the first occurrence of a character in a string.
- `void *memset(void *s, int c, unsigned long n)`: Fills memory with a constant byte.
- `void *memcpy(void *dest, const void *src, unsigned long n)`: Copies memory area.

### 3. Conversion & Output (`mlibc.h` / `itoa.s` / `printf.s`)
- `int atoi(const char *str)`: Converts a string to an integer.
- `char *itoa(int value, char *str, int base)`: Converts an integer to a string (supports multiple bases).
- `void printf(char *format, ...)`: Formatted output to the screen (VGA).

### 4. Hardware Access (`hardware.h` / `hardware.asm`)
Low-level wrappers for CPU I/O instructions.
- `outb`, `inb`, `outw`, `inw`: Standard I/O port operations.
- `insw`, `outsw`: Stream I/O for transferring blocks of data (used in disk drivers).
- `void cinfo(char *buf)`: Retrieves CPU Brand/Vendor string.
- `u64 rinfo(u64 mb_ptr)`: Returns total RAM size in KB (from Multiboot info).

## Characteristics
- **No Dependencies**: The library is self-contained and does not rely on any external environment.
- **Kernel-Mode**: All functions are designed for execution in Ring 0.
