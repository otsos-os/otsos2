/* !DEFINES!

$define %type va_list as compiler variadic argument cursor

*/

/* !SPACE!

$space %export va_list, va_start, va_arg, va_end, va_copy

*/

#ifndef _STDARG_H
#define _STDARG_H
typedef __builtin_va_list va_list;
#define va_start(ap, last)	__builtin_va_start(ap, last)
#define va_arg(ap, type)	__builtin_va_arg(ap, type)
#define va_end(ap)		__builtin_va_end(ap)
#define va_copy(dst, src)	__builtin_va_copy(dst, src)
#endif
