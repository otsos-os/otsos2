/* !DEFINES!

$define %type char as byte string element
$define %func strtol as function with args const char *, char **, int
$define %func exit as procedure with args int

*/

/* !SPACE!

$space %internal digit_value
$space %export atoi, strtol, strtoul, getenv, exit, abort

*/

#include <ctype.h>
#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
digit_value(int c)
{
	if (c >= '0' && c <= '9') {
		return (c - '0');
	}
	if (c >= 'a' && c <= 'z') {
		return (c - 'a' + 10);
	}
	if (c >= 'A' && c <= 'Z') {
		return (c - 'A' + 10);
	}
	return (-1);
}

unsigned long
strtoul(const char *s, char **endptr, int base)
{
	unsigned long	value;
	const char	*p;
	int		neg, digit;

	p = s;
	while (isspace((unsigned char)*p)) {
		p++;
	}

	neg = 0;
	if (*p == '+' || *p == '-') {
		neg = (*p == '-');
		p++;
	}

	if (base == 0) {
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
			base = 16;
			p += 2;
		} else if (p[0] == '0') {
			base = 8;
			p++;
		} else {
			base = 10;
		}
	} else if (base == 16 && p[0] == '0' &&
	    (p[1] == 'x' || p[1] == 'X')) {
		p += 2;
	}

	if (base < 2 || base > 36) {
		errno = EINVAL;
		if (endptr) {
			*endptr = (char *)s;
		}
		return (0);
	}

	value = 0;
	while ((digit = digit_value((unsigned char)*p)) >= 0 &&
	    digit < base) {
		value = value * (unsigned long)base + (unsigned long)digit;
		p++;
	}

	if (endptr) {
		*endptr = (char *)p;
	}
	if (neg) {
		return ((unsigned long)(-(long)value));
	}
	return (value);
}

long
strtol(const char *s, char **endptr, int base)
{
	return ((long)strtoul(s, endptr, base));
}

int
atoi(const char *s)
{
	return ((int)strtol(s, NULL, 10));
}

char *
getenv(const char *name)
{
	char	**env;
	size_t	len;

	if (!name || !environ) {
		return (NULL);
	}
	len = strlen(name);
	for (env = environ; *env; env++) {
		if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
			return (*env + len + 1);
		}
	}
	return (NULL);
}

void
exit(int code)
{
	fflush(NULL);
	procExit(code);
}

void
abort(void)
{
	procExit(134);
}
