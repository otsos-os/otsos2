/* !DEFINES!

$define %type api_sysinfo as struct with kernel identity strings
$define %type api_timeinfo as struct with native time data
$define %func sysInfo as function with args api_sysinfo *

*/

/* !SPACE!

$space %export sysInfo, sysMemInfo, sysCpuInfo, sysRandom
$space %export sysTimeInfo, sysTime

*/

#include <native.h>
#include <stddef.h>
#include "private.h"

int
sysInfo(struct api_sysinfo *buf)
{
	return (__sysret_int(__syscall1(CALL_SYS_INFO, (long)buf)));
}

int
sysMemInfo(struct api_meminfo *buf)
{
	return (__sysret_int(__syscall1(CALL_SYS_MEMINFO, (long)buf)));
}

int
sysCpuInfo(struct api_cpuinfo *buf)
{
	return (__sysret_int(__syscall1(CALL_SYS_CPUINFO, (long)buf)));
}

int
sysRandom(void *buf, size_t len)
{
	if (!__count_ok(len)) {
		return (-1);
	}
	return (__sysret_int(__syscall2(CALL_SYS_RANDOM, (long)buf,
	    (long)len)));
}

int
sysTimeInfo(struct api_timeinfo *buf)
{
	return (__sysret_int(__syscall1(CALL_SYS_TIMEINFO, (long)buf)));
}

int
sysTime(void)
{
	return (__sysret_int(__syscall0(CALL_SYS_TIME)));
}
