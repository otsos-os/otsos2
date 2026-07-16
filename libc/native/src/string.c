/* !DEFINES!

$define %type size_t as native object size
$define %func strlen as function with args const char *
$define %func memcpy as function with args void *, const void *, size_t

*/

/* !SPACE!

$space %export strlen, strnlen, strcmp, strncmp, strcpy, strncpy
$space %export strcat, strchr, strrchr, strstr, memcpy, memmove, memset
$space %export memcmp

*/

#include <stddef.h>
#include <string.h>

size_t
strlen(const char *s)
{
	size_t	len;

	len = 0;
	while (s[len] != '\0') {
		len++;
	}
	return (len);
}

size_t
strnlen(const char *s, size_t maxlen)
{
	size_t	len;

	len = 0;
	while (len < maxlen && s[len] != '\0') {
		len++;
	}
	return (len);
}

int
strcmp(const char *a, const char *b)
{
	while (*a != '\0' && *a == *b) {
		a++;
		b++;
	}
	return ((unsigned char)*a - (unsigned char)*b);
}

int
strncmp(const char *a, const char *b, size_t n)
{
	size_t	i;

	i = 0;
	while (i < n) {
		if (a[i] != b[i] || a[i] == '\0') {
			return ((unsigned char)a[i] - (unsigned char)b[i]);
		}
		i++;
	}
	return (0);
}

char *
strcpy(char *dst, const char *src)
{
	char	*out;

	out = dst;
	while ((*dst++ = *src++) != '\0') {
	}
	return (out);
}

char *
strncpy(char *dst, const char *src, size_t n)
{
	size_t	i;

	i = 0;
	while (i < n && src[i] != '\0') {
		dst[i] = src[i];
		i++;
	}
	while (i < n) {
		dst[i] = '\0';
		i++;
	}
	return (dst);
}

char *
strcat(char *dst, const char *src)
{
	strcpy(dst + strlen(dst), src);
	return (dst);
}

char *
strchr(const char *s, int c)
{
	char	ch;

	ch = (char)c;
	while (*s != '\0') {
		if (*s == ch) {
			return ((char *)s);
		}
		s++;
	}
	if (ch == '\0') {
		return ((char *)s);
	}
	return (NULL);
}

char *
strrchr(const char *s, int c)
{
	const char	*last;
	char		ch;

	last = NULL;
	ch = (char)c;
	while (*s != '\0') {
		if (*s == ch) {
			last = s;
		}
		s++;
	}
	if (ch == '\0') {
		return ((char *)s);
	}
	return ((char *)last);
}

char *
strstr(const char *haystack, const char *needle)
{
	size_t	len;

	if (*needle == '\0') {
		return ((char *)haystack);
	}
	len = strlen(needle);
	while (*haystack != '\0') {
		if (strncmp(haystack, needle, len) == 0) {
			return ((char *)haystack);
		}
		haystack++;
	}
	return (NULL);
}

void *
memcpy(void *dst, const void *src, size_t n)
{
	unsigned char		*d;
	const unsigned char	*s;
	size_t			i;

	d = (unsigned char *)dst;
	s = (const unsigned char *)src;
	for (i = 0; i < n; i++) {
		d[i] = s[i];
	}
	return (dst);
}

void *
memmove(void *dst, const void *src, size_t n)
{
	unsigned char		*d;
	const unsigned char	*s;
	size_t			i;

	d = (unsigned char *)dst;
	s = (const unsigned char *)src;
	if (d < s) {
		for (i = 0; i < n; i++) {
			d[i] = s[i];
		}
	} else if (d > s) {
		i = n;
		while (i > 0) {
			i--;
			d[i] = s[i];
		}
	}
	return (dst);
}

void *
memset(void *dst, int c, size_t n)
{
	unsigned char	*d;
	size_t		i;

	d = (unsigned char *)dst;
	for (i = 0; i < n; i++) {
		d[i] = (unsigned char)c;
	}
	return (dst);
}

int
memcmp(const void *a, const void *b, size_t n)
{
	const unsigned char	*pa;
	const unsigned char	*pb;
	size_t			i;

	pa = (const unsigned char *)a;
	pb = (const unsigned char *)b;
	for (i = 0; i < n; i++) {
		if (pa[i] != pb[i]) {
			return (pa[i] - pb[i]);
		}
	}
	return (0);
}
