/* !DEFINES!

$define %type size_t as native object size
$define %func malloc as function with args size_t
$define %func exit as procedure with args int

*/

/* !SPACE!

$space %export malloc, free, calloc, realloc
$space %export atoi, strtol, strtoul, getenv, exit, abort

*/

#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

extern char **environ;

void	*malloc(size_t size);
void	free(void *ptr);
void	*calloc(size_t nmemb, size_t size);
void	*realloc(void *ptr, size_t size);
int	atoi(const char *s);
double	atof(const char *nptr);
long	strtol(const char *s, char **endptr, int base);
unsigned long strtoul(const char *s, char **endptr, int base);
double	strtod(const char *nptr, char **endptr);
char	*getenv(const char *name);
int	abs(int j);
long	labs(long j);
int	rand(void);
void	srand(unsigned int seed);
void	qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
int	system(const char *command);
int	atexit(void (*function)(void));
void	exit(int code) __attribute__((noreturn));
void	abort(void) __attribute__((noreturn));

#endif
