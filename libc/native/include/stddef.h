/* !DEFINES!

$define %type size_t as native unsigned object size
$define %type ssize_t as native signed object size
$define %type ptrdiff_t as pointer difference integer

*/

/* !SPACE!

$space %export size_t, ssize_t, ptrdiff_t, NULL

*/

#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned long	size_t;
typedef signed long	ssize_t;
typedef signed long	ptrdiff_t;

#ifndef NULL
#define NULL ((void *)0)
#endif
#define offsetof(type, member) __builtin_offsetof(type, member)

#endif
