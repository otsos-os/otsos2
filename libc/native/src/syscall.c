/* !DEFINES!

$define %type long as native syscall integer
$define %func __syscall0 as function with args long
$define %func __syscall4 as function with args long, long, long, long, long
$define %func __syscall5 as function with args long, long, long, long, long, long
$define %func __sysret as function with args long

*/

/* !SPACE!

$space %export __syscall0, __syscall1, __syscall2
$space %export __syscall3, __syscall4, __syscall5
$space %export __sysret, __sysret_int

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
__syscall4(long num, long a1, long a2, long a3, long a4)
{
	register long	r10 __asm__("r10");
	long		ret;

	r10 = a4;
	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10)
	    : "rcx", "r11", "memory");
	return (ret);
}
long
__syscall5(long num, long a1, long a2, long a3, long a4, long a5)
{
	register long	r10 __asm__("r10");
	register long	r8 __asm__("r8");
	long		ret;

	r10 = a4;
	r8 = a5;
	__asm__ volatile("syscall"
	    : "=a"(ret)
	    : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10),
	      "r"(r8)
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
