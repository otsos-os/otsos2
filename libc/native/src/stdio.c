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
$space %export feof, ferror, clearerr

*/

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
out_string(struct fmt_out *out, const char *s, int width, int left)
{
	int	len, pad;

	if (!s) {
		s = "(null)";
	}
	len = (int)strlen(s);
	pad = width - len;
	if (!left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
	while (*s) {
		out_char(out, *s++);
	}
	if (left) {
		while (pad-- > 0) {
			out_char(out, ' ');
		}
	}
}

static void
print_unsigned(struct fmt_out *out, unsigned long long value, int base,
    int upper, int width, int zero)
{
	char		buf[32];
	const char	*digits;
	int		i, pad;

	digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	i = 0;
	if (value == 0) {
		buf[i++] = '0';
	} else {
		while (value != 0) {
			buf[i++] = digits[value % (unsigned)base];
			value /= (unsigned)base;
		}
	}
	pad = width - i;
	while (pad-- > 0) {
		out_char(out, zero ? '0' : ' ');
	}
	while (i-- > 0) {
		out_char(out, buf[i]);
	}
}

static void
print_signed(struct fmt_out *out, long long value, int width, int zero)
{
	unsigned long long	u;

	if (value < 0) {
		out_char(out, '-');
		u = (unsigned long long)(-value);
		if (width > 0) {
			width--;
		}
	} else {
		u = (unsigned long long)value;
	}
	print_unsigned(out, u, 10, 0, width, zero);
}

static int
format_core(struct fmt_out *out, const char *fmt, va_list ap)
{
	unsigned long long	uval;
	long long		sval;
	const char		*p;
	int			width, left, zero, long_count;
	int			spec;

	p = fmt;
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

		left = 0;
		zero = 0;
		if (*p == '-') {
			left = 1;
			p++;
		}
		if (*p == '0') {
			zero = 1;
			p++;
		}

		width = 0;
		while (*p >= '0' && *p <= '9') {
			width = width * 10 + (*p - '0');
			p++;
		}

		long_count = 0;
		while (*p == 'l') {
			long_count++;
			p++;
		}

		spec = (unsigned char)*p++;
		switch (spec) {
		case 'c':
			out_char(out, (char)va_arg(ap, int));
			break;
		case 's':
			out_string(out, va_arg(ap, const char *), width, left);
			break;
		case 'd':
		case 'i':
			if (long_count >= 2) {
				sval = va_arg(ap, long long);
			} else if (long_count == 1) {
				sval = va_arg(ap, long);
			} else {
				sval = va_arg(ap, int);
			}
			print_signed(out, sval, width, zero);
			break;
		case 'u':
		case 'x':
		case 'X':
			if (long_count >= 2) {
				uval = va_arg(ap, unsigned long long);
			} else if (long_count == 1) {
				uval = va_arg(ap, unsigned long);
			} else {
				uval = va_arg(ap, unsigned int);
			}
			print_unsigned(out, uval, spec == 'u' ? 10 : 16,
			    spec == 'X', width, zero);
			break;
		case 'p':
			out_string(out, "0x", 0, 0);
			uval = (uintptr_t)va_arg(ap, void *);
			print_unsigned(out, uval, 16, 0, width, 0);
			break;
		default:
			out_char(out, '%');
			out_char(out, (char)spec);
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
