/* !DEFINES!

$define %type api_kofo_info as struct with native KOFO module metadata
$define %func kofoLoad as function with args const char *, uint32_t
$define %func kofoInfo as function with args uint32_t, api_kofo_info *
$define %func kofoUnload as function with args uint32_t, uint32_t

*/

/* !SPACE!

$space %export kofoLoad, kofoInfo, kofoUnload

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

int
kofoLoad(const char *path, uint32_t flags)
{
	return (__sysret_int(__syscall2(CALL_KOFO_LOAD, (long)path,
	    (long)flags)));
}

int
kofoInfo(uint32_t id, struct api_kofo_info *info)
{
	return (__sysret_int(__syscall2(CALL_KOFO_INFO, (long)id,
	    (long)info)));
}

int
kofoUnload(uint32_t id, uint32_t flags)
{
	return (__sysret_int(__syscall2(CALL_KOFO_UNLOAD, (long)id,
	    (long)flags)));
}
