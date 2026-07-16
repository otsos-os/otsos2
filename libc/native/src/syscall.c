/* !DEFINES!

$define %type long as native syscall integer
$define %func __syscall0 as function with args long
$define %func __sysret as function with args long

*/

/* !SPACE!

$space %export __syscall0, __syscall1, __syscall2
$space %export __syscall3, __sysret, __sysret_int

*/

#include <errno.h>
#include "private.h"

long
__syscall0(long num)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num)
	    : "rcx", "r11", "memory");
	return (ret);
}

long
__syscall1(long num, long a1)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1)
	    : "rcx", "r11", "memory");
	return (ret);
}

long
__syscall2(long num, long a1, long a2)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2)
	    : "rcx", "r11", "memory");
	return (ret);
}

long
__syscall3(long num, long a1, long a2, long a3)
{
	long	ret;

	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2), "d"(a3)
	    : "rcx", "r11", "memory");
	return (ret);
}

long
__sysret(long ret)
{
	if (ret < 0) {
		errno = (int)-ret;
		return (-1);
	}
	return (ret);
}

int
__sysret_int(long ret)
{
	return ((int)__sysret(ret));
}
