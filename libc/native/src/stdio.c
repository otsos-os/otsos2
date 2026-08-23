/* !DEFINES!

$define %type FILE as C stream over terminal or native data handle
$define %type fmt_out as formatted output sink
$define %func printf as function with args const char *, ...
$define %func fflush as function with args FILE *

*/

/* !SPACE!

$space %internal raw_read, raw_write, flush_write, fill_read, put_byte
$space %internal out_char, out_string, print_unsigned, print_signed
$space %internal format_core, mode_flags, stream_init
$space %export stdin, stdout, stderr, printf, fprintf, vprintf, vfprintf
$space %export snprintf, vsnprintf, putchar, puts, fopen, fclose, fflush
$space %export fread, fwrite, fgetc, fputc, fgets, fseek, ftell
$space %export feof, ferror, clearerr, sscanf, vsscanf, remove, rename

*/

#include <ctype.h>
#include <errno.h>
#include <native.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_READ	0x0001
#define FILE_WRITE	0x0002
#define FILE_TERM	0x0004
#define FILE_OWNBUF	0x0008
#define FILE_LINEBUF	0x0010
#define FILE_UNBUF	0x0020

struct FILE {
	int		fd;
	int		flags;
	int		eof;
	int		err;
	unsigned char	*buf;
	size_t		cap;
	size_t		rpos;
	size_t		rlen;
	size_t		wpos;
};

struct fmt_out {
	FILE	*stream;
	char	*buf;
	size_t	size;
	size_t	pos;
	size_t	total;
	int	string;
};

static unsigned char	stdin_buf[BUFSIZ];
static unsigned char	stdout_buf[BUFSIZ];

static FILE	stdin_obj = {
	-1, FILE_READ | FILE_TERM, 0, 0, stdin_buf, BUFSIZ, 0, 0, 0
};
static FILE	stdout_obj = {
	-1, FILE_WRITE | FILE_TERM | FILE_LINEBUF, 0, 0,
	stdout_buf, BUFSIZ, 0, 0, 0
};
static FILE	stderr_obj = {
	-1, FILE_WRITE | FILE_TERM | FILE_UNBUF, 0, 0, 0, 0, 0, 0, 0
};

FILE	*stdin = &stdin_obj;
FILE	*stdout = &stdout_obj;
FILE	*stderr = &stderr_obj;

static void
stream_init(FILE *stream, int fd, int flags, unsigned char *buf, size_t cap)
{
	stream->fd = fd;
	stream->flags = flags;
	stream->eof = 0;
	stream->err = 0;
	stream->buf = buf;
	stream->cap = cap;
	stream->rpos = 0;
	stream->rlen = 0;
	stream->wpos = 0;
}

static ssize_t
raw_read(FILE *stream, void *buf, size_t len)
{
	if (stream->flags & FILE_TERM) {
		return (termRead(buf, len));
	}
	return (dataRead(stream->fd, buf, len));
}

static int
raw_write(FILE *stream, const void *buf, size_t len)
{
	const char	*p;
	size_t		done;
	ssize_t		n;

	p = (const char *)buf;
	done = 0;
	while (done < len) {
		if (stream->flags & FILE_TERM) {
			n = termWrite(p + done, len - done);
		} else {
			n = dataWrite(stream->fd, p + done, len - done);
		}
		if (n <= 0) {
			stream->err = 1;
			return (EOF);
		}
		done += (size_t)n;
	}
	return (0);
}

static int
flush_write(FILE *stream)
{
	if (!stream || !(stream->flags & FILE_WRITE)) {
		errno = EINVAL;
		return (EOF);
	}
	if (stream->wpos == 0) {
		return (0);
	}
	if (raw_write(stream, stream->buf, stream->wpos) != 0) {
		return (EOF);
	}
	stream->wpos = 0;
	return (0);
}

static int
fill_read(FILE *stream)
{
	ssize_t	n;

	if (!stream || !(stream->flags & FILE_READ)) {
		errno = EBADF;
		return (EOF);
	}
	if (!stream->buf || stream->cap == 0) {
		errno = EINVAL;
		return (EOF);
	}
	n = raw_read(stream, stream->buf, stream->cap);
	if (n < 0) {
		stream->err = 1;
		return (EOF);
	}
	if (n == 0) {
		stream->eof = 1;
		return (EOF);
	}
	stream->rpos = 0;
	stream->rlen = (size_t)n;
	return (0);
}

static int
put_byte(FILE *stream, unsigned char c)
{
	if (!stream || !(stream->flags & FILE_WRITE)) {
		errno = EBADF;
		return (EOF);
	}
	if ((stream->flags & FILE_UNBUF) || !stream->buf || stream->cap == 0) {
		return (raw_write(stream, &c, 1) == 0 ? c : EOF);
	}
	stream->buf[stream->wpos++] = c;
	if (stream->wpos == stream->cap ||
	    ((stream->flags & FILE_LINEBUF) && c == '\n')) {
		if (flush_write(stream) != 0) {
			return (EOF);
		}
	}
	return (c);
}

static void
out_char(struct fmt_out *out, char c)
{
	if (out->string) {
		if (out->buf && out->pos + 1 < out->size) {
			out->buf[out->pos] = c;
		}
		out->pos++;
	} else {
		fputc((unsigned char)c, out->stream);
	}
	out->total++;
}

static void
out_string(struct fmt_out *out, const char *s, int width, int prec, int left)
{
	int	len, pad, i;

	if (!s) {
		s = "(null)";
	}
	len = (int)strlen(s);
	if (prec >= 0 && prec < len) {
		len = prec;
	}
	pad = width - len;
	if (!left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
	for (i = 0; i < len; i++) {
		out_char(out, s[i]);
	}
	if (left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
}

static void
print_number(struct fmt_out *out, unsigned long long value, int is_signed,
    long long svalue, int base, int upper, int width, int prec, int left,
    int zero, int plus, int space, int alt)
{
	char		buf[64];
	const char	*digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int		num_digits = 0;
	int		zero_pad = 0;
	int		prefix_len = 0;
	int		total_digits = 0;
	int		pad = 0;
	char		sign_char = 0;
	const char	*prefix = "";

	if (is_signed) {
		if (svalue < 0) {
			sign_char = '-';
			value = (unsigned long long)(-svalue);
		} else {
			value = (unsigned long long)svalue;
			if (plus) {
				sign_char = '+';
			} else if (space) {
				sign_char = ' ';
			}
		}
	}

	if (alt) {
		if (base == 16 && value != 0) {
			prefix = upper ? "0X" : "0x";
		} else if (base == 8 && value != 0) {
			prefix = "0";
		}
	}

	if (value == 0) {
		if (prec != 0) {
			buf[num_digits++] = '0';
		}
	} else {
		while (value != 0) {
			buf[num_digits++] = digits[value % (unsigned)base];
			value /= (unsigned)base;
		}
	}

	if (prec >= 0) {
		zero = 0;
		if (prec > num_digits) {
			zero_pad = prec - num_digits;
		}
	}

	prefix_len = (sign_char ? 1 : 0) + (int)strlen(prefix);
	total_digits = num_digits + zero_pad;
	pad = width - (prefix_len + total_digits);

	if (zero && pad > 0 && !left) {
		zero_pad += pad;
		pad = 0;
	}

	if (!left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}

	if (sign_char) {
		out_char(out, sign_char);
	}
	while (*prefix) {
		out_char(out, *prefix++);
	}

	while (zero_pad-- > 0) {
		out_char(out, '0');
	}
	while (num_digits-- > 0) {
		out_char(out, buf[num_digits]);
	}

	if (left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
}

static void
print_float(struct fmt_out *out, double val, int width, int prec, int left,
    int zero, int plus, int space)
{
	char			sign_char = 0;
	char			buf[128];
	char			int_buf[32];
	unsigned long long	int_part;
	double			frac_part;
	int			len, pad, int_digits, idx, i;

	if (prec < 0) {
		prec = 6;
	}

	if (val < 0.0) {
		sign_char = '-';
		val = -val;
	} else if (plus) {
		sign_char = '+';
	} else if (space) {
		sign_char = ' ';
	}

	int_part = (unsigned long long)val;
	frac_part = val - (double)int_part;

	int_digits = 0;
	if (int_part == 0) {
		int_buf[int_digits++] = '0';
	} else {
		while (int_part != 0) {
			int_buf[int_digits++] = (char)('0' + (int_part % 10));
			int_part /= 10;
		}
	}

	idx = 0;
	while (int_digits-- > 0) {
		buf[idx++] = int_buf[int_digits];
	}

	if (prec > 0) {
		buf[idx++] = '.';
		for (i = 0; i < prec; i++) {
			int digit;
			frac_part *= 10.0;
			digit = (int)frac_part;
			if (digit > 9) {
				digit = 9;
			}
			buf[idx++] = (char)('0' + digit);
			frac_part -= (double)digit;
		}
	}
	buf[idx] = '\0';
	len = idx + (sign_char ? 1 : 0);
	pad = width - len;

	if (zero && pad > 0 && !left) {
		if (sign_char) {
			out_char(out, sign_char);
			sign_char = 0;
		}
		while (pad-- > 0) {
			out_char(out, '0');
		}
	} else if (!left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}

	if (sign_char) {
		out_char(out, sign_char);
	}
	for (i = 0; i < idx; i++) {
		out_char(out, buf[i]);
	}

	if (left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
}

static int
format_core(struct fmt_out *out, const char *fmt, va_list ap)
{
	const char	*p = fmt;

	while (*p) {
		if (*p != '%') {
			out_char(out, *p++);
			continue;
		}
		p++;
		if (*p == '%') {
			out_char(out, *p++);
			continue;
		}

		int left = 0, plus = 0, space = 0, zero = 0, alt = 0;
		for (;;) {
			if (*p == '-') left = 1;
			else if (*p == '+') plus = 1;
			else if (*p == ' ') space = 1;
			else if (*p == '0') zero = 1;
			else if (*p == '#') alt = 1;
			else break;
			p++;
		}

		int width = 0;
		if (*p == '*') {
			width = va_arg(ap, int);
			if (width < 0) {
				left = 1;
				width = -width;
			}
			p++;
		} else {
			while (isdigit((unsigned char)*p)) {
				width = width * 10 + (*p - '0');
				p++;
			}
		}

		int prec = -1;
		if (*p == '.') {
			p++;
			prec = 0;
			if (*p == '*') {
				prec = va_arg(ap, int);
				p++;
			} else {
				while (isdigit((unsigned char)*p)) {
					prec = prec * 10 + (*p - '0');
					p++;
				}
			}
		}

		int len_mod = 0;
		if (*p == 'h') {
			p++;
			if (*p == 'h') {
				len_mod = 2;
				p++;
			} else {
				len_mod = 1;
			}
		} else if (*p == 'l') {
			p++;
			if (*p == 'l') {
				len_mod = 4;
				p++;
			} else {
				len_mod = 3;
			}
		} else if (*p == 'z') {
			len_mod = 5;
			p++;
		} else if (*p == 'j') {
			len_mod = 6;
			p++;
		}

		int spec = (unsigned char)*p++;
		switch (spec) {
		case 'c': {
			char c = (char)va_arg(ap, int);
			int pad = width - 1;
			if (!left) {
				while (pad-- > 0) out_char(out, ' ');
			}
			out_char(out, c);
			if (left) {
				while (pad-- > 0) out_char(out, ' ');
			}
			break;
		}
		case 's': {
			const char *s = va_arg(ap, const char *);
			out_string(out, s, width, prec, left);
			break;
		}
		case 'd':
		case 'i': {
			long long sval;
			if (len_mod == 4 || len_mod == 6) {
				sval = va_arg(ap, long long);
			} else if (len_mod == 3 || len_mod == 5) {
				sval = va_arg(ap, long);
			} else if (len_mod == 1) {
				sval = (short)va_arg(ap, int);
			} else if (len_mod == 2) {
				sval = (signed char)va_arg(ap, int);
			} else {
				sval = va_arg(ap, int);
			}

			print_number(out, 0, 1, sval, 10, 0, width, prec, left, zero, plus, space, 0);
			break;
		}
		case 'u':
		case 'o':
		case 'x':
		case 'X': {
			unsigned long long uval;
			int base;
			if (len_mod == 4 || len_mod == 6) {
				uval = va_arg(ap, unsigned long long);
			} else if (len_mod == 3 || len_mod == 5) {
				uval = va_arg(ap, unsigned long);
			} else if (len_mod == 1) {
				uval = (unsigned short)va_arg(ap, unsigned int);
			} else if (len_mod == 2) {
				uval = (unsigned char)va_arg(ap, unsigned int);
			} else {
				uval = va_arg(ap, unsigned int);
			}

			base = (spec == 'u') ? 10 : (spec == 'o' ? 8 : 16);
			print_number(out, uval, 0, 0, base, spec == 'X', width, prec, left, zero, 0, 0, alt);
			break;
		}
		case 'p': {
			void *ptr = va_arg(ap, void *);
			if (ptr == NULL) {
				out_string(out, "(nil)", width, prec, left);
			} else {
				print_number(out, (uintptr_t)ptr, 0, 0, 16, 0, width, prec, left, 0, 0, 0, 1);
			}
			break;
		}
		case 'f':
		case 'F': {
			double fval = va_arg(ap, double);
			print_float(out, fval, width, prec, left, zero, plus, space);
			break;
		}
		case 'n': {
			int *nptr = va_arg(ap, int *);
			if (nptr) *nptr = (int)out->total;
			break;
		}
		default:
			out_char(out, '%');
			if (spec) out_char(out, (char)spec);
			break;
		}
	}
	return ((int)out->total);
}

int
vfprintf(FILE *stream, const char *fmt, va_list ap)
{
	struct fmt_out	out;

	if (!stream || !fmt) {
		errno = EINVAL;
		return (-1);
	}
	memset(&out, 0, sizeof(out));
	out.stream = stream;
	return (format_core(&out, fmt, ap));
}

int
fprintf(FILE *stream, const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);
	ret = vfprintf(stream, fmt, ap);
	va_end(ap);
	return (ret);
}

int
vprintf(const char *fmt, va_list ap)
{
	return (vfprintf(stdout, fmt, ap));
}

int
printf(const char *fmt, ...)
{
	va_list	ap;
	int	ret;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);
	return (ret);
}

int
vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
	struct fmt_out	out;
	int		ret;

	if (!fmt) {
		errno = EINVAL;
		return (-1);
	}
	memset(&out, 0, sizeof(out));
	out.buf = buf;
	out.size = size;
	out.string = 1;
	ret = format_core(&out, fmt, ap);
	if (buf && size > 0) {
		if (out.pos >= size) {
			buf[size - 1] = '\0';
		} else {
			buf[out.pos] = '\0';
		}
	}
	return (ret);
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

int
putchar(int c)
{
	return (fputc(c, stdout));
}

int
puts(const char *s)
{
	if (fwrite(s, 1, strlen(s), stdout) != strlen(s)) {
		return (EOF);
	}
	if (fputc('\n', stdout) == EOF) {
		return (EOF);
	}
	return (0);
}

static int
mode_flags(const char *mode)
{
	int	flags;
	int	plus;

	if (!mode || !mode[0]) {
		errno = EINVAL;
		return (-1);
	}

	plus = strchr(mode, '+') != 0;
	switch (mode[0]) {
	case 'r':
		flags = plus ? API_OPEN_RW : API_OPEN_READ;
		break;
	case 'w':
		flags = plus ? API_OPEN_RW : API_OPEN_WRITE;
		flags |= API_OPEN_CREATE | API_OPEN_TRUNC;
		break;
	case 'a':
		flags = plus ? API_OPEN_RW : API_OPEN_WRITE;
		flags |= API_OPEN_CREATE | API_OPEN_APPEND;
		break;
	default:
		errno = EINVAL;
		return (-1);
	}
	return (flags);
}

FILE *
fopen(const char *path, const char *mode)
{
	unsigned char	*buf;
	FILE		*stream;
	int		flags, fd;

	flags = mode_flags(mode);
	if (flags < 0) {
		return (0);
	}
	fd = dataOpen(path, flags);
	if (fd < 0) {
		return (0);
	}
	stream = malloc(sizeof(*stream));
	if (!stream) {
		dataClose(fd);
		return (0);
	}
	buf = malloc(BUFSIZ);
	if (!buf) {
		free(stream);
		dataClose(fd);
		return (0);
	}
	stream_init(stream, fd, FILE_OWNBUF, buf, BUFSIZ);
	if (flags & API_OPEN_READ) {
		stream->flags |= FILE_READ;
	}
	if (flags & API_OPEN_WRITE) {
		stream->flags |= FILE_WRITE;
	}
	return (stream);
}

int
fclose(FILE *stream)
{
	int	ret;

	if (!stream) {
		errno = EINVAL;
		return (EOF);
	}
	ret = fflush(stream);
	if (!(stream->flags & FILE_TERM)) {
		if (dataClose(stream->fd) < 0) {
			ret = EOF;
		}
		if (stream->flags & FILE_OWNBUF) {
			free(stream->buf);
		}
		free(stream);
	}
	return (ret);
}

int
fflush(FILE *stream)
{
	int	ret;

	if (!stream) {
		ret = 0;
		if (flush_write(stdout) != 0) {
			ret = EOF;
		}
		if (flush_write(stderr) != 0) {
			ret = EOF;
		}
		return (ret);
	}
	if (stream->flags & FILE_WRITE) {
		return (flush_write(stream));
	}
	stream->rpos = 0;
	stream->rlen = 0;
	return (0);
}

size_t
fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	unsigned char	*p;
	size_t		total, done;

	if (!stream || !(stream->flags & FILE_READ)) {
		errno = EBADF;
		return (0);
	}
	if (size == 0 || nmemb == 0) {
		return (0);
	}
	total = size * nmemb;
	p = (unsigned char *)ptr;
	done = 0;
	while (done < total) {
		if (stream->rpos >= stream->rlen && fill_read(stream) != 0) {
			break;
		}
		p[done++] = stream->buf[stream->rpos++];
	}
	return (done / size);
}

size_t
fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	const unsigned char	*p;
	size_t			total, done;

	if (!stream || !(stream->flags & FILE_WRITE)) {
		errno = EBADF;
		return (0);
	}
	if (size == 0 || nmemb == 0) {
		return (0);
	}
	total = size * nmemb;
	p = (const unsigned char *)ptr;
	done = 0;
	while (done < total) {
		if (put_byte(stream, p[done]) == EOF) {
			break;
		}
		done++;
	}
	return (done / size);
}

int
fgetc(FILE *stream)
{
	unsigned char	c;

	if (fread(&c, 1, 1, stream) != 1) {
		return (EOF);
	}
	return (c);
}

int
fputc(int c, FILE *stream)
{
	return (put_byte(stream, (unsigned char)c));
}

char *
fgets(char *s, int size, FILE *stream)
{
	int	i, c;

	if (!s || size <= 0) {
		errno = EINVAL;
		return (0);
	}
	i = 0;
	while (i + 1 < size) {
		c = fgetc(stream);
		if (c == EOF) {
			break;
		}
		s[i++] = (char)c;
		if (c == '\n') {
			break;
		}
	}
	s[i] = '\0';
	if (i == 0) {
		return (0);
	}
	return (s);
}

int
fseek(FILE *stream, long offset, int whence)
{
	if (!stream || (stream->flags & FILE_TERM)) {
		errno = ESPIPE;
		return (-1);
	}
	if (stream->flags & FILE_WRITE) {
		if (fflush(stream) != 0) {
			return (-1);
		}
	}
	stream->rpos = 0;
	stream->rlen = 0;
	if (dataSeek(stream->fd, offset, whence) < 0) {
		stream->err = 1;
		return (-1);
	}
	stream->eof = 0;
	return (0);
}

long
ftell(FILE *stream)
{
	long	pos;

	if (!stream || (stream->flags & FILE_TERM)) {
		errno = ESPIPE;
		return (-1);
	}
	if (stream->flags & FILE_WRITE) {
		if (fflush(stream) != 0) {
			return (-1);
		}
	}
	pos = dataSeek(stream->fd, 0, SEEK_CUR);
	if (pos < 0) {
		stream->err = 1;
		return (-1);
	}
	if (stream->flags & FILE_READ) {
		pos -= (long)(stream->rlen - stream->rpos);
	}
	return (pos);
}

int
feof(FILE *stream)
{
	return (stream ? stream->eof : 0);
}

int
ferror(FILE *stream)
{
	return (stream ? stream->err : 1);
}

void
clearerr(FILE *stream)
{
	if (stream) {
		stream->eof = 0;
		stream->err = 0;
	}
}

int
remove(const char *pathname)
{
	return (fsUnlink(pathname));
}

int
rename(const char *oldpath, const char *newpath)
{
	return (fsRename(oldpath, newpath));
}

int
vsscanf(const char *str, const char *fmt, va_list ap)
{
	int		count;
	const char	*s;
	const char	*f;
	int		nconv;

	if (str == NULL || fmt == NULL) {
		return (EOF);
	}

	count = 0;
	nconv = 0;
	s = str;
	f = fmt;

	while (*f != '\0') {
		if (isspace((unsigned char)*f)) {
			while (isspace((unsigned char)*f)) {
				f++;
			}
			while (isspace((unsigned char)*s)) {
				s++;
			}
			continue;
		}

		if (*f != '%') {
			if (*s != *f) {
				break;
			}
			s++;
			f++;
			continue;
		}

		f++;
		if (*f == '%') {
			if (*s != '%') {
				break;
			}
			s++;
			f++;
			continue;
		}

		int suppress = 0;
		if (*f == '*') {
			suppress = 1;
			f++;
		}

		int width = 0;
		while (isdigit((unsigned char)*f)) {
			width = width * 10 + (*f - '0');
			f++;
		}
		if (width <= 0) {
			width = -1;
		}

		int len_mod = 0;
		if (*f == 'h') {
			f++;
			if (*f == 'h') {
				len_mod = 2;
				f++;
			} else {
				len_mod = 1;
			}
		} else if (*f == 'l') {
			f++;
			if (*f == 'l') {
				len_mod = 4;
				f++;
			} else {
				len_mod = 3;
			}
		} else if (*f == 'z') {
			len_mod = 5;
			f++;
		}

		if (*f == 'n') {
			f++;
			if (!suppress) {
				int *p = va_arg(ap, int *);
				if (p != NULL) {
					*p = (int)(s - str);
				}
			}
			continue;
		}

		if (*f != 'c' && *f != '[') {
			while (isspace((unsigned char)*s)) {
				s++;
			}
		}

		if (*s == '\0') {
			break;
		}

		nconv++;

		if (*f == 'd' || *f == 'i') {
			int base = (*f == 'd') ? 10 : 0;
			char buf[64];
			const char *start = s;
			char *end;
			long long val;

			if (width > 0 && width < (int)sizeof(buf) - 1) {
				int i = 0;
				if (*s == '+' || *s == '-') {
					buf[i++] = *s++;
				}
				while (*s && i < width && (isdigit((unsigned char)*s) || (base != 10 && isxdigit((unsigned char)*s)))) {
					buf[i++] = *s++;
				}
				buf[i] = '\0';
				val = strtol(buf, &end, base);
				if (end == buf) {
					s = start;
					break;
				}
			} else {
				val = strtol(s, &end, base);
				if (end == s) {
					break;
				}
				s = end;
			}

			if (!suppress) {
				if (len_mod == 2) {
					char *p = va_arg(ap, char *);
					if (p) *p = (char)val;
				} else if (len_mod == 1) {
					short *p = va_arg(ap, short *);
					if (p) *p = (short)val;
				} else if (len_mod == 4) {
					long long *p = va_arg(ap, long long *);
					if (p) *p = (long long)val;
				} else if (len_mod == 3 || len_mod == 5) {
					long *p = va_arg(ap, long *);
					if (p) *p = (long)val;
				} else {
					int *p = va_arg(ap, int *);
					if (p) *p = (int)val;
				}
				count++;
			}
			f++;
		} else if (*f == 'u' || *f == 'x' || *f == 'X' || *f == 'o' || *f == 'p') {
			int base = 10;
			if (*f == 'x' || *f == 'X' || *f == 'p') {
				base = 16;
			} else if (*f == 'o') {
				base = 8;
			}

			char buf[64];
			const char *start = s;
			char *end;
			unsigned long long val;

			if (width > 0 && width < (int)sizeof(buf) - 1) {
				int i = 0;
				if (*s == '+' || *s == '-') {
					buf[i++] = *s++;
				}
				if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && i + 2 <= width) {
					buf[i++] = *s++;
					buf[i++] = *s++;
				}
				while (*s && i < width && (isdigit((unsigned char)*s) || (base == 16 && isxdigit((unsigned char)*s)))) {
					buf[i++] = *s++;
				}
				buf[i] = '\0';
				val = strtoul(buf, &end, base);
				if (end == buf) {
					s = start;
					break;
				}
			} else {
				val = strtoul(s, &end, base);
				if (end == s) {
					break;
				}
				s = end;
			}

			if (!suppress) {
				if (*f == 'p') {
					void **p = va_arg(ap, void **);
					if (p) *p = (void *)(uintptr_t)val;
				} else if (len_mod == 2) {
					unsigned char *p = va_arg(ap, unsigned char *);
					if (p) *p = (unsigned char)val;
				} else if (len_mod == 1) {
					unsigned short *p = va_arg(ap, unsigned short *);
					if (p) *p = (unsigned short)val;
				} else if (len_mod == 4) {
					unsigned long long *p = va_arg(ap, unsigned long long *);
					if (p) *p = (unsigned long long)val;
				} else if (len_mod == 3 || len_mod == 5) {
					unsigned long *p = va_arg(ap, unsigned long *);
					if (p) *p = (unsigned long)val;
				} else {
					unsigned int *p = va_arg(ap, unsigned int *);
					if (p) *p = (unsigned int)val;
				}
				count++;
			}
			f++;
		} else if (*f == 'f' || *f == 'e' || *f == 'E' || *f == 'g' || *f == 'G') {
			char buf[128];
			const char *start = s;
			char *end;
			double val;

			if (width > 0 && width < (int)sizeof(buf) - 1) {
				int i = 0;
				while (*s && i < width && !isspace((unsigned char)*s)) {
					buf[i++] = *s++;
				}
				buf[i] = '\0';
				val = strtod(buf, &end);
				if (end == buf) {
					s = start;
					break;
				}
			} else {
				val = strtod(s, &end);
				if (end == s) {
					break;
				}
				s = end;
			}

			if (!suppress) {
				if (len_mod == 3) {
					double *p = va_arg(ap, double *);
					if (p) *p = val;
				} else {
					float *p = va_arg(ap, float *);
					if (p) *p = (float)val;
				}
				count++;
			}
			f++;
		} else if (*f == 's') {
			char *dst = suppress ? NULL : va_arg(ap, char *);
			int len = 0;

			while (*s != '\0' && !isspace((unsigned char)*s) && (width < 0 || len < width)) {
				if (dst != NULL) {
					dst[len] = *s;
				}
				len++;
				s++;
			}
			if (len == 0) {
				break;
			}
			if (dst != NULL) {
				dst[len] = '\0';
				count++;
			}
			f++;
		} else if (*f == 'c') {
			char *dst = suppress ? NULL : va_arg(ap, char *);
			int w = (width > 0) ? width : 1;
			int len = 0;

			while (*s != '\0' && len < w) {
				if (dst != NULL) {
					dst[len] = *s;
				}
				len++;
				s++;
			}
			if (len < w) {
				break;
			}
			if (dst != NULL) {
				count++;
			}
			f++;
		} else if (*f == '[') {
			f++;
			int invert = 0;
			if (*f == '^') {
				invert = 1;
				f++;
			}
			char set[256];
			memset(set, 0, sizeof(set));
			if (*f == ']') {
				set[(unsigned char)']'] = 1;
				f++;
			}
			while (*f != '\0' && *f != ']') {
				if (*(f + 1) == '-' && *(f + 2) != '\0' && *(f + 2) != ']') {
					unsigned char c1 = (unsigned char)*f;
					unsigned char c2 = (unsigned char)*(f + 2);
					if (c1 > c2) {
						unsigned char tmp = c1;
						c1 = c2;
						c2 = tmp;
					}
					for (unsigned int c = c1; c <= c2; c++) {
						set[c] = 1;
					}
					f += 3;
				} else {
					set[(unsigned char)*f] = 1;
					f++;
				}
			}
			if (*f == ']') {
				f++;
			}

			char *dst = suppress ? NULL : va_arg(ap, char *);
			int len = 0;

			while (*s != '\0' && (width < 0 || len < width)) {
				int match = set[(unsigned char)*s];
				if (invert) match = !match;
				if (!match) break;
				if (dst != NULL) {
					dst[len] = *s;
				}
				len++;
				s++;
			}
			if (len == 0) break;
			if (dst != NULL) {
				dst[len] = '\0';
				count++;
			}
		} else {
			break;
		}
	}

	if (nconv == 0 && *s == '\0') {
		return (EOF);
	}
	return (count);
}

int
sscanf(const char *str, const char *fmt, ...)
{
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = vsscanf(str, fmt, ap);
	va_end(ap);
	return (ret);
}


