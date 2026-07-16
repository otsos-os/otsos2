/* !DEFINES!

$define %type int as native data handle
$define %func dataOpen as function with args const char *, int
$define %func dataRead as function with args int, void *, size_t

*/

/* !SPACE!

$space %export dataOpen, dataClose, dataRead, dataWrite, dataReadFull
$space %export dataWriteFull, dataSeek, dataPipe

*/

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include "private.h"

int
dataOpen(const char *path, int flags)
{
	return (__sysret_int(__syscall2(CALL_DATA_OPEN, (long)path,
	    (long)flags)));
}

int
dataClose(int handle)
{
	return (__sysret_int(__syscall1(CALL_DATA_CLOSE, (long)handle)));
}

ssize_t
dataRead(int handle, void *buf, size_t count)
{
	if (!__count_ok(count)) {
		return (-1);
	}
	return (__sysret(__syscall3(CALL_DATA_READ, (long)handle,
	    (long)buf, (long)count)));
}

ssize_t
dataWrite(int handle, const void *buf, size_t count)
{
	if (!__count_ok(count)) {
		return (-1);
	}
	return (__sysret(__syscall3(CALL_DATA_WRITE, (long)handle,
	    (long)buf, (long)count)));
}

int
dataReadFull(int handle, void *buf, size_t count)
{
	char	*p;
	size_t	done;
	ssize_t	n;

	p = (char *)buf;
	done = 0;
	while (done < count) {
		n = dataRead(handle, p + done, count - done);
		if (n < 0) {
			return (-1);
		}
		if (n == 0) {
			errno = EIO;
			return (-1);
		}
		done += (size_t)n;
	}
	return (0);
}

int
dataWriteFull(int handle, const void *buf, size_t count)
{
	const char	*p;
	size_t		done;
	ssize_t		n;

	p = (const char *)buf;
	done = 0;
	while (done < count) {
		n = dataWrite(handle, p + done, count - done);
		if (n <= 0) {
			return (-1);
		}
		done += (size_t)n;
	}
	return (0);
}

long
dataSeek(int handle, long offset, int whence)
{
	return (__sysret(__syscall3(CALL_DATA_SEEK, (long)handle,
	    offset, (long)whence)));
}

int
dataPipe(int handles[2])
{
	return (__sysret_int(__syscall1(CALL_DATA_PIPE, (long)handles)));
}
