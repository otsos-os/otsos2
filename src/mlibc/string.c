#include <mlibc/mlibc.h>

int strcmp(const char *str1, const char *str2) {
  while (*str1 && (*str1 == *str2)) {
    str1++;
    str2++;
  }
  return *(const unsigned char *)str1 - *(const unsigned char *)str2;
}

int atoi(const char *str) {
  int result = 0;
  int sign = 1;

  if (*str == '-') {
    sign = -1;
    str++;
  }

  while (*str >= '0' && *str <= '9') {
    result = result * 10 + (*str - '0');
    str++;
  }

  return sign * result;
}
int strlen(const char *str) {
  int len = 0;
  while (str[len] != '\0') {
    len++;
  }
  return len;
}

char* strcpy(char* dest, const char* src) {
  char* original_dest = dest;
  while ((*dest++ = *src++));
  return original_dest;
}

char* strcat(char* dest, const char* src) {
  char* original_dest = dest;
  while (*dest) dest++;
  while ((*dest++ = *src++));
  return original_dest;
}

char* strchr(const char* str, int c) {
  while (*str) {
    if (*str == c) {
      return (char*)str;
    }
    str++;
  }
  if (c == 0) {
    return (char*)str;
  }
  return 0;
}
void *memset(void *s, int c, unsigned long n) {
  unsigned char *p = (unsigned char *)s;
  while (n--)
    *p++ = (unsigned char)c;
  return s;
}
void *memcpy(void *dest, const void *src, unsigned long n) {
  unsigned char *d = (unsigned char *)dest;
  const unsigned char *s = (const unsigned char *)src;
  while (n--)
    *d++ = *s++;
  return dest;
}
