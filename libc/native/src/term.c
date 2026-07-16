/* !DEFINES!

$define %type api_term_info as struct with terminal size and state
$define %func termWrite as function with args const void *, size_t
$define %func termRead as function with args void *, size_t

*/

/* !SPACE!

$space %export termRead, termReadFlags, termWrite, termPrint
$space %export termInfo, termPower

*/

#include <errno.h>
#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "private.h"

ssize_t
termRead(void *buf, size_t count)
{
	return (termReadFlags(buf, count, 0));
}

ssize_t
termReadFlags(void *buf, size_t count, uint32_t flags)
{
	if (!__count_ok(count)) {
		return (-1);
	}
	return (__sysret(__syscall3(CALL_TERM_READ, (long)buf,
	    (long)count, (long)flags)));
}

ssize_t
termWrite(const void *buf, size_t count)
{
	if (!__count_ok(count)) {
		return (-1);
	}
	return (__sysret(__syscall3(CALL_TERM_WRITE, (long)buf,
	    (long)count, 0)));
}

ssize_t
termPrint(const char *text)
{
	size_t	done, len;
	ssize_t	n;

	if (!text) {
		errno = EINVAL;
		return (-1);
	}
	done = 0;
	len = strlen(text);
	while (done < len) {
		n = termWrite(text + done, len - done);
		if (n <= 0) {
			return (-1);
		}
		done += (size_t)n;
	}
	return ((ssize_t)done);
}

int
termInfo(struct api_term_info *info)
{
	return (__sysret_int(__syscall1(CALL_TERM_INFO, (long)info)));
}

int
termPower(struct api_term_power *args)
{
	return (__sysret_int(__syscall1(CALL_TERM_POWER, (long)args)));
}
