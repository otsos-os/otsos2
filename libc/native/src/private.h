/* !DEFINES!

$define %type long as native syscall integer
$define %type size_t as native object size
$define %func __syscall4 as function with args long x5
$define %func __syscall5 as function with args long x6
$define %func __syscall6 as function with args long x7
$define %func __sysret as function with args long

*/

/* !SPACE!

$space %export __syscall0, __syscall1, __syscall2, __syscall3
$space %export __syscall4, __syscall5, __syscall6
$space %export __sysret, __sysret_int, __count_ok

*/

#ifndef LIBC_PRIVATE_H
#define LIBC_PRIVATE_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

long	__syscall0(long num);
long	__syscall1(long num, long a1);
long	__syscall2(long num, long a1, long a2);
long	__syscall3(long num, long a1, long a2, long a3);
long	__syscall4(long num, long a1, long a2, long a3, long a4);
long	__syscall5(long num, long a1, long a2, long a3, long a4,
	    long a5);
long	__syscall6(long num, long a1, long a2, long a3, long a4,
	    long a5, long a6);
long	__sysret(long ret);
int	__sysret_int(long ret);

static inline int
__count_ok(size_t count)
{
	if (count > UINT32_MAX) {
		errno = EINVAL;
		return (0);
	}
	return (1);
}

#endif
