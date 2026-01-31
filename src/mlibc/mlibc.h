#ifndef MLIBC_H
#define MLIBC_H

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

int strcmp(const char *str1, const char *str2);
int strlen(const char *str);
int atoi(const char *str);
char *strcpy(char *dest, const char *src);
char *strcat(char *dest, const char *src);
char *strchr(const char *str, int c);
char *itoa(int value, char *str, int base);


void *memset(void *s, int c, unsigned long n);
void *memcpy(void *dest, const void *src, unsigned long n);

void outb(unsigned short port, unsigned char data);
unsigned char inb(unsigned short port);
void outw(unsigned short port, unsigned short data);
unsigned short inw(unsigned short port);
void insw(unsigned short port, void *addr, unsigned long count);
void outsw(unsigned short port, void *addr, unsigned long count);

#include <mlibc/memory.h>

#endif