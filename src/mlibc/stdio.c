/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <kernel/cm/cm.h>
#include <kernel/drivers/uart/uart.h>
#include <mlibc/stdio.h>

static int	log_enabled = 1;
static int	log_drivers = 1;
static int	log_initialized = 0;
static void	(*terminal_mirror)(char) = NULL;

void
stdio_init(void)
{
	if (log_initialized) {
		return;
	}
	if (cm_is_initialized()) {
		log_enabled = cm_get_bool_default("SYSTEM", "Log",
		    "Enabled", 1);
		log_drivers = cm_get_bool_default("SYSTEM", "Log",
		    "Drivers", 1);
	}
	log_initialized = 1;
}

void
stdio_set_terminal_mirror(void (*callback)(char))
{
	terminal_mirror = callback;
}

static void
log_emit_format(const char *fmt, __builtin_va_list args)
{
	char	buffer[512];
	int	i;

	vsnprintf(buffer, sizeof(buffer), fmt, args);
	uart_write_string(buffer);
	if (terminal_mirror) {
		for (i = 0; buffer[i] != '\0'; i++) {
			terminal_mirror(buffer[i]);
		}
	}
}

void
printk(const char *fmt, ...)
{
	__builtin_va_list	args;

	if (!log_enabled) {
		return;
	}
	__builtin_va_start(args, fmt);
	log_emit_format(fmt, args);
	__builtin_va_end(args);
}

void
drivers_log(const char *fmt, ...)
{
	__builtin_va_list	args;
	char			buffer[512];

	if (!log_enabled || !log_drivers) {
		return;
	}
	__builtin_va_start(args, fmt);
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	__builtin_va_end(args);
	printk("[driver] %s", buffer);
}

void
klog(const char *fmt, ...)
{
	__builtin_va_list	args;

	__builtin_va_start(args, fmt);
	log_emit_format(fmt, args);
	__builtin_va_end(args);
}

static int
format_uint(char *buf, size_t size, u64 value, int base, int upper)
{
	const char	*digits;
	char		tmp[64];
	int		i, n, written;

	digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	if (value == 0) {
		tmp[0] = '0';
		n = 1;
	} else {
		n = 0;
		while (value > 0) {
			tmp[n++] = digits[value % (u64)base];
			value /= (u64)base;
		}
	}
	written = 0;
	for (i = n - 1; i >= 0; i--) {
		if (written < (int)size - 1 && size > 0) {
			buf[written] = tmp[i];
		}
		written++;
	}
	if (size > 0) {
		if (written < (int)size) {
			buf[written] = '\0';
		} else {
			buf[size - 1] = '\0';
		}
	}
	return (written);
}

static int
format_padded_uint(char *buf, size_t size, u64 value, int base,
    int upper, int width, int zero_pad)
{
	const char	*digits;
	char		tmp[64];
	int		i, n, padding, written;

	digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	if (value == 0) {
		tmp[0] = '0';
		n = 1;
	} else {
		n = 0;
		while (value > 0) {
			tmp[n++] = digits[value % (u64)base];
			value /= (u64)base;
		}
	}
	padding = width > n ? width - n : 0;
	written = 0;
	for (i = 0; i < padding; i++) {
		if (written < (int)size - 1 && size > 0) {
			buf[written] = zero_pad ? '0' : ' ';
		}
		written++;
	}
	for (i = n - 1; i >= 0; i--) {
		if (written < (int)size - 1 && size > 0) {
			buf[written] = tmp[i];
		}
		written++;
	}
	if (size > 0) {
		if (written < (int)size) {
			buf[written] = '\0';
		} else {
			buf[size - 1] = '\0';
		}
	}
	return (written);
}

static int
format_int(char *buf, size_t size, s64 value, int base)
{
	int	written;
	u64	uval;

	written = 0;
	if (value < 0) {
		if (written < (int)size - 1 && size > 0) {
			buf[written] = '-';
		}
		written++;
		uval = (u64)(-value);
	} else {
		uval = (u64)value;
	}
	written += format_uint(buf + written, size > (size_t)written ?
	    size - written : 0, uval, base, 0);
	return (written);
}

int
vsnprintf(char *str, size_t size, const char *fmt,
    __builtin_va_list args)
{
	const char	*p;
	int		written, total;
	char		*out;
	int		out_size, width, zero_pad;

	written = 0;
	total = 0;
	p = fmt;

	while (*p) {
		if (*p != '%') {
			if ((size_t)written < size - 1 && size > 0) {
				str[written] = *p;
			}
			written++;
			total++;
			p++;
			continue;
		}

		p++;
		zero_pad = 0;
		width = 0;
		if (*p == '0') {
			zero_pad = 1;
			p++;
		}
		while (*p >= '0' && *p <= '9') {
			width = width * 10 + (*p - '0');
			p++;
		}

		out = str + written;
		out_size = (int)size - written;
		if (out_size < 0) {
			out_size = 0;
		}

		switch (*p) {
		case 's': {
			const char	*s;
			int		len, i;

			s = __builtin_va_arg(args, const char *);
			if (!s) {
				s = "(null)";
			}
			len = 0;
			while (s[len]) {
				len++;
			}
			for (i = 0; i < len; i++) {
				if ((size_t)written < size - 1 && size > 0) {
					str[written] = s[i];
				}
				written++;
			}
			total += len;
			break;
		}
		case 'c': {
			char	c;

			c = (char)__builtin_va_arg(args, int);
			if ((size_t)written < size - 1 && size > 0) {
				str[written] = c;
			}
			written++;
			total++;
			break;
		}
		case 'd':
		case 'i': {
			int	len;

			len = format_int(out, (size_t)out_size,
			    __builtin_va_arg(args, int), 10);
			written += len;
			total += len;
			break;
		}
		case 'u': {
			int	len;

			len = format_padded_uint(out, (size_t)out_size,
			    __builtin_va_arg(args, unsigned int), 10, 0,
			    width, zero_pad);
			written += len;
			total += len;
			break;
		}
		case 'x':
		case 'X': {
			int	len;

			len = format_padded_uint(out, (size_t)out_size,
			    __builtin_va_arg(args, unsigned int), 16,
			    *p == 'X', width, zero_pad);
			written += len;
			total += len;
			break;
		}
		case 'p': {
			void	*ptr;
			int	len, i;
			const char prefix[] = "0x";

			ptr = __builtin_va_arg(args, void *);
			for (i = 0; prefix[i]; i++) {
				if ((size_t)written < size - 1 && size > 0) {
					str[written] = prefix[i];
				}
				written++;
			}
			out = str + written;
			out_size = (int)size - written;
			if (out_size < 0) {
				out_size = 0;
			}
			len = format_uint(out, (size_t)out_size,
			    (u64)(unsigned long)ptr, 16, 0);
			written += len;
			total += 2 + len;
			break;
		}
		case 'l': {
			p++;
			if (*p == 'l') {
				p++;
				if (*p == 'd' || *p == 'i') {
					int	len;

					len = format_int(out, (size_t)out_size,
					    __builtin_va_arg(args, long long),
					    10);
					written += len;
					total += len;
				} else if (*p == 'u') {
					int	len;

					len = format_uint(out, (size_t)out_size,
					    __builtin_va_arg(args, unsigned long long),
					    10, 0);
					written += len;
					total += len;
				} else if (*p == 'x' || *p == 'X') {
					int	len;

					len = format_uint(out, (size_t)out_size,
					    __builtin_va_arg(args, unsigned long long),
					    16, *p == 'X');
					written += len;
					total += len;
				} else {
					goto literal;
				}
			} else if (*p == 'd' || *p == 'i') {
				int	len;

				len = format_int(out, (size_t)out_size,
				    __builtin_va_arg(args, long), 10);
				written += len;
				total += len;
			} else if (*p == 'u') {
				int	len;

				len = format_uint(out, (size_t)out_size,
				    __builtin_va_arg(args, unsigned long), 10, 0);
				written += len;
				total += len;
			} else if (*p == 'x' || *p == 'X') {
				int	len;

				len = format_uint(out, (size_t)out_size,
				    __builtin_va_arg(args, unsigned long), 16,
				    *p == 'X');
				written += len;
				total += len;
			} else {
				goto literal;
			}
			break;
		}
		case 'z': {
			if (*(p + 1) == 'd' || *(p + 1) == 'i') {
				int	len;

				len = format_int(out, (size_t)out_size,
				    __builtin_va_arg(args, long long), 10);
				written += len;
				total += len;
			} else if (*(p + 1) == 'u') {
				int	len;

				len = format_uint(out, (size_t)out_size,
				    __builtin_va_arg(args, unsigned long long),
				    10, 0);
				written += len;
				total += len;
			} else if (*(p + 1) == 'x' || *(p + 1) == 'X') {
				int	len;

				len = format_uint(out, (size_t)out_size,
				    __builtin_va_arg(args, unsigned long long),
				    16, *(p + 1) == 'X');
				written += len;
				total += len;
			} else {
				goto literal;
			}
			p++;
			break;
		}
		case '%':
		literal:
		default:
			if ((size_t)written < size - 1 && size > 0) {
				str[written] = *p ? *p : '%';
			}
			written++;
			total++;
			break;
		}
		if (*p) {
			p++;
		}
	}

	if (size > 0) {
		if ((size_t)written < size) {
			str[written] = '\0';
		} else {
			str[size - 1] = '\0';
		}
	}
	return (total);
}

int
snprintf(char *str, size_t size, const char *fmt, ...)
{
	__builtin_va_list	args;
	int			ret;

	__builtin_va_start(args, fmt);
	ret = vsnprintf(str, size, fmt, args);
	__builtin_va_end(args);
	return (ret);
}
