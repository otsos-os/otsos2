/* !DEFINES!

$define %type size_t as native object size
$define %func strlen as function with args const char *
$define %func memcpy as function with args void *, const void *, size_t
$define %func memset as function with args void *, int, size_t

*/

/* !SPACE!

$space %export strlen, strcmp, strncmp, strcpy, strncpy, strcat, strchr
$space %export strrchr, strstr, memcpy, memmove, memset, memcmp

*/

#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

size_t	strlen(const char *s);
size_t	strnlen(const char *s, size_t maxlen);
int	strcmp(const char *a, const char *b);
int	strncmp(const char *a, const char *b, size_t n);
char	*strcpy(char *dst, const char *src);
char	*strncpy(char *dst, const char *src, size_t n);
char	*strcat(char *dst, const char *src);
char	*strchr(const char *s, int c);
char	*strrchr(const char *s, int c);
char	*strstr(const char *haystack, const char *needle);
void	*memcpy(void *dst, const void *src, size_t n);
void	*memmove(void *dst, const void *src, size_t n);
void	*memset(void *dst, int c, size_t n);
int	memcmp(const void *a, const void *b, size_t n);
char	*strdup(const char *s);
char	*strndup(const char *s, size_t n);
int	strcasecmp(const char *a, const char *b);
int	strncasecmp(const char *a, const char *b, size_t n);

#endif
