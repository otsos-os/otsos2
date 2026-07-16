/* !DEFINES!

$define %type FILE as native C stream object
$define %func printf as function with args const char *, ...
$define %func fopen as function with args const char *, const char *

*/

/* !SPACE!

$space %export FILE, stdin, stdout, stderr
$space %export printf, fprintf, snprintf, vprintf, vfprintf, vsnprintf
$space %export fopen, fclose, fread, fwrite, fgetc, fputc, fgets, puts

*/

#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <stddef.h>

#define EOF	(-1)
#define BUFSIZ	1024

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef struct FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int	printf(const char *fmt, ...);
int	fprintf(FILE *stream, const char *fmt, ...);
int	vprintf(const char *fmt, va_list ap);
int	vfprintf(FILE *stream, const char *fmt, va_list ap);
int	snprintf(char *buf, size_t size, const char *fmt, ...);
int	vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);
int	putchar(int c);
int	puts(const char *s);
FILE	*fopen(const char *path, const char *mode);
int	fclose(FILE *stream);
int	fflush(FILE *stream);
size_t	fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t	fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int	fgetc(FILE *stream);
int	fputc(int c, FILE *stream);
char	*fgets(char *s, int size, FILE *stream);
int	fseek(FILE *stream, long offset, int whence);
long	ftell(FILE *stream);
int	feof(FILE *stream);
int	ferror(FILE *stream);
void	clearerr(FILE *stream);

#endif
