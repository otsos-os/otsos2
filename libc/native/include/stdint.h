/* !DEFINES!

$define %type int*_t as fixed width signed integers
$define %type uint*_t as fixed width unsigned integers
$define %type uintptr_t as integer large enough for a pointer

*/

/* !SPACE!

$space %export int8_t, int16_t, int32_t, int64_t
$space %export uint8_t, uint16_t, uint32_t, uint64_t
$space %export intptr_t, uintptr_t

*/

#ifndef _STDINT_H
#define _STDINT_H

typedef signed char		int8_t;
typedef signed short		int16_t;
typedef signed int		int32_t;
typedef signed long		int64_t;

typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long		uint64_t;

typedef signed long		intptr_t;
typedef unsigned long		uintptr_t;

#define INT8_MIN	(-128)
#define INT16_MIN	(-32767 - 1)
#define INT32_MIN	(-2147483647 - 1)
#define INT64_MIN	(-9223372036854775807L - 1L)

#define INT8_MAX	127
#define INT16_MAX	32767
#define INT32_MAX	2147483647
#define INT64_MAX	9223372036854775807L

#define UINT8_MAX	255U
#define UINT16_MAX	65535U
#define UINT32_MAX	4294967295U
#define UINT64_MAX	18446744073709551615UL
#define SIZE_MAX	UINT64_MAX

#endif
