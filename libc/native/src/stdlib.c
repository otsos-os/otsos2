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

#define ATEXIT_MAX 32
static void (*s_atexit_funcs[ATEXIT_MAX])(void);
static int s_atexit_count = 0;

int
atexit(void (*function)(void))
{
	if (function == NULL || s_atexit_count >= ATEXIT_MAX) {
		return (-1);
	}
	s_atexit_funcs[s_atexit_count++] = function;
	return (0);
}

void
exit(int code)
{
	while (s_atexit_count > 0) {
		s_atexit_count--;
		if (s_atexit_funcs[s_atexit_count] != NULL) {
			s_atexit_funcs[s_atexit_count]();
		}
	}
	fflush(NULL);
	procExit(code);
}

void
abort(void)
{
	procExit(134);
}

double
strtod(const char *s, char **endptr)
{
	const char	*p;
	double		result, sign, factor;
	int		has_digits;

	if (s == NULL) {
		if (endptr != NULL) {
			*endptr = NULL;
		}
		return (0.0);
	}

	p = s;
	while (isspace((unsigned char)*p)) {
		p++;
	}

	sign = 1.0;
	if (*p == '+' || *p == '-') {
		if (*p == '-') {
			sign = -1.0;
		}
		p++;
	}

	if (strncasecmp(p, "nan", 3) == 0) {
		p += 3;
		if (endptr != NULL) {
			*endptr = (char *)p;
		}
		return (0.0 / 0.0);
	}
	if (strncasecmp(p, "infinity", 8) == 0) {
		p += 8;
		if (endptr != NULL) {
			*endptr = (char *)p;
		}
		return (sign * (1.0 / 0.0));
	}
	if (strncasecmp(p, "inf", 3) == 0) {
		p += 3;
		if (endptr != NULL) {
			*endptr = (char *)p;
		}
		return (sign * (1.0 / 0.0));
	}

	result = 0.0;
	has_digits = 0;
	while (isdigit((unsigned char)*p)) {
		result = result * 10.0 + (*p - '0');
		has_digits = 1;
		p++;
	}

	if (*p == '.') {
		p++;
		factor = 0.1;
		while (isdigit((unsigned char)*p)) {
			result += (*p - '0') * factor;
			factor *= 0.1;
			has_digits = 1;
			p++;
		}
	}

	if (!has_digits) {
		if (endptr != NULL) {
			*endptr = (char *)s;
		}
		return (0.0);
	}

	if (*p == 'e' || *p == 'E') {
		const char	*exp_p;
		int		exp_sign, exp_val, has_exp_digits, e;
		double		scale, base;

		exp_p = p + 1;
		exp_sign = 1;
		if (*exp_p == '+' || *exp_p == '-') {
			if (*exp_p == '-') {
				exp_sign = -1;
			}
			exp_p++;
		}

		exp_val = 0;
		has_exp_digits = 0;
		while (isdigit((unsigned char)*exp_p)) {
			exp_val = exp_val * 10 + (*exp_p - '0');
			has_exp_digits = 1;
			exp_p++;
		}

		if (has_exp_digits) {
			scale = 1.0;
			base = 10.0;
			e = exp_val;
			while (e > 0) {
				if (e & 1) {
					scale *= base;
				}
				base *= base;
				e >>= 1;
			}
			if (exp_sign < 0) {
				result /= scale;
			} else {
				result *= scale;
			}
			p = exp_p;
		}
	}

	if (endptr != NULL) {
		*endptr = (char *)p;
	}

	return (sign * result);
}

double
atof(const char *s)
{
	return (strtod(s, NULL));
}

int
system(const char *command)
{
	const char	*argv[4];
	int		pid, status;

	if (command == NULL) {
		return (1);
	}

	argv[0] = "/bin/sh";
	argv[1] = "-c";
	argv[2] = command;
	argv[3] = NULL;

	pid = procSpawnNative("/bin/sh", (char *const *)argv, NULL);
	if (pid < 0) {
		return (-1);
	}

	status = 0;
	for (;;) {
		int got = procWait(&status);
		if (got < 0) {
			return (-1);
		}
		if (got == pid) {
			break;
		}
	}

	return (status);
}

int
abs(int j)
{
	return (j < 0 ? -j : j);
}

long
labs(long j)
{
	return (j < 0 ? -j : j);
}

static unsigned long s_next_rand = 1;

int
rand(void)
{
	s_next_rand = s_next_rand * 1103515245 + 12345;
	return ((int)((s_next_rand / 65536) % 32768));
}

void
srand(unsigned int seed)
{
	s_next_rand = (unsigned long)seed;
}

static void
swap_bytes(char *a, char *b, size_t size)
{
	size_t	i;
	char	tmp;

	for (i = 0; i < size; i++) {
		tmp = a[i];
		a[i] = b[i];
		b[i] = tmp;
	}
}

static void
qsort_internal(char *base, size_t size, int (*compar)(const void *, const void *),
    size_t low, size_t high)
{
	size_t	mid, i, j;
	char	*pivot;

	while (low < high) {
		if (high - low < 16) {
			for (i = low + 1; i <= high; i++) {
				for (j = i; j > low; j--) {
					if (compar(base + (j - 1) * size, base + j * size) > 0) {
						swap_bytes(base + (j - 1) * size, base + j * size, size);
					} else {
						break;
					}
				}
			}
			break;
		}

		mid = low + (high - low) / 2;
		if (compar(base + low * size, base + mid * size) > 0) {
			swap_bytes(base + low * size, base + mid * size, size);
		}
		if (compar(base + low * size, base + high * size) > 0) {
			swap_bytes(base + low * size, base + high * size, size);
		}
		if (compar(base + mid * size, base + high * size) > 0) {
			swap_bytes(base + mid * size, base + high * size, size);
		}

		swap_bytes(base + mid * size, base + (high - 1) * size, size);
		pivot = base + (high - 1) * size;

		i = low;
		j = high - 1;
		while (1) {
			do {
				i++;
			} while (compar(base + i * size, pivot) < 0);
			do {
				j--;
			} while (j > low && compar(base + j * size, pivot) > 0);
			if (i >= j) {
				break;
			}
			swap_bytes(base + i * size, base + j * size, size);
		}

		swap_bytes(base + i * size, base + (high - 1) * size, size);

		if (i > 0 && (i - 1 - low) < (high - i)) {
			if (i > 0) {
				qsort_internal(base, size, compar, low, i - 1);
			}
			low = i + 1;
		} else {
			qsort_internal(base, size, compar, i + 1, high);
			if (i == 0) {
				break;
			}
			high = i - 1;
		}
	}
}

void
qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	if (base == NULL || nmemb < 2 || size == 0 || compar == NULL) {
		return;
	}
	qsort_internal((char *)base, size, compar, 0, nmemb - 1);
}

