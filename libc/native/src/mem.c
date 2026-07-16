/* !DEFINES!

$define %type mem_map_args as struct with native memory map request
$define %func memMap as function with args const mem_map_args *

*/

/* !SPACE!

$space %export memMap, memUnmap

*/

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include "private.h"

void *
memMap(const struct mem_map_args *args)
{
	long	ret;

	if (!args) {
		errno = EINVAL;
		return (0);
	}
	ret = __syscall1(CALL_MEM_MAP, (long)args);
	if (ret < 0) {
		errno = (int)-ret;
		return (0);
	}
	return ((void *)ret);
}

int
memUnmap(void *addr, size_t length)
{
	return (__sysret_int(__syscall2(CALL_MEM_UNMAP, (long)addr,
	    (long)length)));
}
