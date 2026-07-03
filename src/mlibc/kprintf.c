/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <mlibc/kprintf.h>
#include <mlibc/mlibc.h>
#include <lib/com1.h>
#include <kernel/console.h>
#include <kernel/other/config.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* Log state -- defaults to all-on before kprintf_init() reads config */
/* ------------------------------------------------------------------ */

int	log_enabled = 1;
int	log_drivers = 1;

/* ------------------------------------------------------------------ */
/* Output callback type and contexts                                   */
/* ------------------------------------------------------------------ */

typedef void	(*out_fn)(void *ctx, char c);

struct buf_state {
	char	*buf;
	size_t	 size;
	size_t	 pos;
};

static void
buf_out(void *ctx, char c)
{
	struct buf_state	*bs = (struct buf_state *)ctx;

	if (bs->pos + 1 < bs->size) {
		bs->buf[bs->pos] = c;
	}
	bs->pos++;
}

static void
serial_out(void *ctx, char c)
{
	(void)ctx;
	com1_write_byte((u8)c);
}

static void
console_out(void *ctx, char c)
{
	(void)ctx;
	console_putchar(c);
}

static void
serial_console_out(void *ctx, char c)
{
	(void)ctx;
	com1_write_byte((u8)c);
	console_putchar(c);
}

static void
emit_pad(out_fn out, void *ctx, char pad, int n)
{
	int	i;

	for (i = 0; i < n; i++) {
		out(ctx, pad);
	}
}

static int
u_to_str(u64 val, int base, int upper, char *buf)
{
	const char	*digits = upper ?
	    "0123456789ABCDEF" : "0123456789abcdef";
	int		n = 0;

	if (val == 0) {
		buf[n++] = '0';
		return (n);
	}
	while (val > 0) {
		buf[n++] = digits[val % (u64)base];
		val /= (u64)base;
	}
	return (n);
}


static void
emit_num(out_fn out, void *ctx, u64 val, int base, int upper,
    char sign_char, int width, int precision, int left_align,
    int zero_pad, int hash)
{
	char	digits[32];
	int	ndig, pre_len, total, pad, i;
	char	prefix[4];

	ndig = u_to_str(val, base, upper, digits);

	if (precision > ndig) {
		int	extra = precision - ndig;
		for (i = ndig - 1; i >= 0; i--) {
			digits[i + extra] = digits[i];
		}
		for (i = 0; i < extra; i++) {
			digits[i] = '0';
		}
		ndig = precision;
	}

	pre_len = 0;
	if (sign_char != 0) {
		prefix[pre_len++] = sign_char;
	}
	if (hash && val != 0 && base == 16) {
		prefix[pre_len++] = '0';
		prefix[pre_len++] = upper ? 'X' : 'x';
	} else if (hash && base == 8 &&
	    (ndig == 0 || digits[ndig - 1] != '0')) {
		prefix[pre_len++] = '0';
	}

	total = pre_len + ndig;

	pad = width - total;
	if (pad < 0) {
		pad = 0;
	}

	if (left_align) {
		for (i = 0; i < pre_len; i++) {
			out(ctx, prefix[i]);
		}
		for (i = ndig - 1; i >= 0; i--) {
			out(ctx, digits[i]);
		}
		emit_pad(out, ctx, ' ', pad);
		return;
	}

	if (zero_pad && precision < 0) {
		for (i = 0; i < pre_len; i++) {
			out(ctx, prefix[i]);
		}
		emit_pad(out, ctx, '0', pad);
		for (i = ndig - 1; i >= 0; i--) {
			out(ctx, digits[i]);
		}
		return;
	}

	emit_pad(out, ctx, ' ', pad);
	for (i = 0; i < pre_len; i++) {
		out(ctx, prefix[i]);
	}
	for (i = ndig - 1; i >= 0; i--) {
		out(ctx, digits[i]);
	}
}


static void
kvprintf_core(const char *fmt, va_list ap, out_fn out, void *ctx)
{
	const char	*p = fmt;

	while (*p) {
		if (*p != '%') {
			out(ctx, *p++);
			continue;
		}

		p++;
		int	left_align = 0;
		int	zero_pad = 0;
		int	show_sign = 0;
		int	space_flag = 0;
		int	hash = 0;

		for (;;) {
			switch (*p) {
			case '-':
				left_align = 1;
				p++;
				continue;
			case '0':
				zero_pad = 1;
				p++;
				continue;
			case '+':
				show_sign = 1;
				p++;
				continue;
			case ' ':
				space_flag = 1;
				p++;
				continue;
			case '#':
				hash = 1;
				p++;
				continue;
			default:
				break;
			}
			break;
		}

		int	width = 0;
		if (*p == '*') {
			width = va_arg(ap, int);
			if (width < 0) {
				left_align = 1;
				width = -width;
			}
			p++;
		} else {
			while (*p >= '0' && *p <= '9') {
				width = width * 10 + (*p - '0');
				p++;
			}
		}

		int	precision = -1;
		if (*p == '.') {
			p++;
			precision = 0;
			if (*p == '*') {
				precision = va_arg(ap, int);
				p++;
			} else {
				while (*p >= '0' && *p <= '9') {
					precision = precision * 10 +
					    (*p - '0');
					p++;
				}
			}
		}

		int	lm_long = 0;
		int	lm_longlong = 0;
		int	lm_size = 0;

		if (*p == 'l') {
			p++;
			if (*p == 'l') {
				lm_longlong = 1;
				p++;
			} else {
				lm_long = 1;
			}
		} else if (*p == 'z') {
			lm_size = 1;
			p++;
		} else if (*p == 'h') {
			p++;
			if (*p == 'h') {
				p++;
			}
		}

		char	conv = *p;
		p++;

		switch (conv) {
		case '%':
			out(ctx, '%');
			break;

		case 'c': {
			char	c = (char)va_arg(ap, int);

			if (!left_align && width > 1) {
				emit_pad(out, ctx, ' ', width - 1);
			}
			out(ctx, c);
			if (left_align && width > 1) {
				emit_pad(out, ctx, ' ', width - 1);
			}
			break;
		}

		case 's': {
			const char	*s = va_arg(ap, const char *);
			int		slen = 0;

			if (!s) {
				s = "(null)";
			}
			while (s[slen]) {
				slen++;
			}
			if (precision >= 0 && precision < slen) {
				slen = precision;
			}

			if (!left_align && width > slen) {
				emit_pad(out, ctx, ' ', width - slen);
			}
			for (int i = 0; i < slen; i++) {
				out(ctx, s[i]);
			}
			if (left_align && width > slen) {
				emit_pad(out, ctx, ' ', width - slen);
			}
			break;
		}

		case 'd':
		case 'i': {
			s64	val;
			char	sign_char = 0;

			if (lm_longlong) {
				val = (s64)va_arg(ap, long long);
			} else if (lm_long) {
				val = (s64)va_arg(ap, long);
			} else if (lm_size) {
				val = (s64)va_arg(ap, size_t);
			} else {
				val = (s64)va_arg(ap, int);
			}

			if (val < 0) {
				sign_char = '-';
				val = -val;
			} else if (show_sign) {
				sign_char = '+';
			} else if (space_flag) {
				sign_char = ' ';
			}

			emit_num(out, ctx, (u64)val, 10, 0, sign_char,
			    width, precision, left_align, zero_pad, 0);
			break;
		}

		case 'u': {
			u64	val;

			if (lm_longlong) {
				val = (u64)va_arg(ap, unsigned long long);
			} else if (lm_long) {
				val = (u64)va_arg(ap, unsigned long);
			} else if (lm_size) {
				val = (u64)va_arg(ap, size_t);
			} else {
				val = (u64)va_arg(ap, unsigned int);
			}

			emit_num(out, ctx, val, 10, 0, 0, width,
			    precision, left_align, zero_pad, 0);
			break;
		}

		case 'x': {
			u64	val;

			if (lm_longlong) {
				val = (u64)va_arg(ap, unsigned long long);
			} else if (lm_long) {
				val = (u64)va_arg(ap, unsigned long);
			} else if (lm_size) {
				val = (u64)va_arg(ap, size_t);
			} else {
				val = (u64)va_arg(ap, unsigned int);
			}

			emit_num(out, ctx, val, 16, 0, 0, width,
			    precision, left_align, zero_pad, hash);
			break;
		}

		case 'X': {
			u64	val;

			if (lm_longlong) {
				val = (u64)va_arg(ap, unsigned long long);
			} else if (lm_long) {
				val = (u64)va_arg(ap, unsigned long);
			} else if (lm_size) {
				val = (u64)va_arg(ap, size_t);
			} else {
				val = (u64)va_arg(ap, unsigned int);
			}

			emit_num(out, ctx, val, 16, 1, 0, width,
			    precision, left_align, zero_pad, hash);
			break;
		}

		case 'o': {
			u64	val;

			if (lm_longlong) {
				val = (u64)va_arg(ap, unsigned long long);
			} else if (lm_long) {
				val = (u64)va_arg(ap, unsigned long);
			} else if (lm_size) {
				val = (u64)va_arg(ap, size_t);
			} else {
				val = (u64)va_arg(ap, unsigned int);
			}

			emit_num(out, ctx, val, 8, 0, 0, width,
			    precision, left_align, zero_pad, hash);
			break;
		}

		case 'p': {
			void	*ptr = va_arg(ap, void *);
			u64	val = (u64)ptr;

			out(ctx, '0');
			out(ctx, 'x');
			char	digits[32];
			int	ndig = u_to_str(val, 16, 0, digits);
			int	pad = 16 - ndig;
			if (pad < 0) {
				pad = 0;
			}
			emit_pad(out, ctx, '0', pad);
			for (int i = ndig - 1; i >= 0; i--) {
				out(ctx, digits[i]);
			}
			break;
		}

		default:
			out(ctx, '%');
			out(ctx, conv);
			break;
		}
	}
}

int
vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct buf_state	bs;

	bs.buf = buf;
	bs.size = size ? size : 1;
	bs.pos = 0;
	kvprintf_core(fmt, ap, buf_out, &bs);
	if (size > 0) {
		int	end = (int)bs.pos;
		if (end >= (int)size) {
			end = (int)size - 1;
		}
		buf[end] = '\0';
	}
	return ((int)bs.pos);
}

int
snprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);
	ret = vsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return (ret);
}

void
printf(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	kvprintf_core(fmt, ap, console_out, NULL);
	va_end(ap);
}

void
kprintf(const char *fmt, ...)
{
	va_list	ap;

	if (!log_enabled) {
		return;
	}
	va_start(ap, fmt);
	kvprintf_core(fmt, ap, serial_console_out, NULL);
	va_end(ap);
}

void
vkprintf(const char *fmt, va_list ap)
{
	if (!log_enabled) {
		return;
	}
	kvprintf_core(fmt, ap, serial_console_out, NULL);
}

void
klog(const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	kvprintf_core(fmt, ap, serial_console_out, NULL);
	va_end(ap);
}

void
vklog(const char *fmt, va_list ap)
{
	kvprintf_core(fmt, ap, serial_console_out, NULL);
}

void
driver_printf(const char *fmt, ...)
{
	va_list	ap;

	if (!log_enabled || !log_drivers) {
		return;
	}
	va_start(ap, fmt);
	kvprintf_core(fmt, ap, serial_out, NULL);
	va_end(ap);
}

void
kprintf_init(void)
{
	log_enabled = config_get_bool("log", "enabled", 1);
	log_drivers = config_get_bool("log", "drivers", 1);
}
