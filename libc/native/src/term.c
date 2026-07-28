/* !DEFINES!

$define %type api_term_info as struct with terminal size and state
$define %type api_term_mode as struct with terminal line discipline flags
$define %type api_term_mouse as struct with console mouse state args
$define %func termWrite as function with args const void *, size_t
$define %func termRead as function with args void *, size_t
$define %func termMouse as function with args struct api_term_mouse *
$define %func termMode as function with args struct api_term_mode *
$define %func termGetMode as function with args struct api_term_mode *
$define %func termSetMode as function with args const struct api_term_mode *
$define %func termEnterRaw as function with args struct api_term_mode *
$define %func termRestoreMode as function with args const struct api_term_mode *

*/

/* !SPACE!

$space %export termRead, termReadFlags, termWrite, termPrint
$space %export termInfo, termPower, termMouse, termMode
$space %export termGetMode, termSetMode
$space %export termEnterRaw, termRestoreMode

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

int
termMouse(struct api_term_mouse *args)
{
	return (__sysret_int(__syscall1(CALL_TERM_MOUSE, (long)args)));
}

int
termMode(struct api_term_mode *args)
{
	return (__sysret_int(__syscall1(CALL_TERM_MODE, (long)args)));
}

int
termGetMode(struct api_term_mode *mode)
{
	if (!mode) {
		errno = EINVAL;
		return (-1);
	}
	memset(mode, 0, sizeof(*mode));
	mode->op = API_TERM_MODE_GET;
	mode->tty = API_TERM_ACTIVE;
	return (termMode(mode));
}

int
termSetMode(const struct api_term_mode *mode)
{
	struct api_term_mode	args;

	if (!mode) {
		errno = EINVAL;
		return (-1);
	}
	memcpy(&args, mode, sizeof(args));
	args.op = API_TERM_MODE_SET;
	return (termMode(&args));
}

int
termEnterRaw(struct api_term_mode *saved)
{
	struct api_term_mode	mode, raw;
	int			ret;

	ret = termGetMode(&mode);
	if (ret != 0) {
		return (ret);
	}
	if (saved) {
		memcpy(saved, &mode, sizeof(*saved));
	}
	memcpy(&raw, &mode, sizeof(raw));
	raw.iflag &= ~(API_TERM_IFLAG_BRKINT | API_TERM_IFLAG_ICRNL |
	    API_TERM_IFLAG_INPCK | API_TERM_IFLAG_ISTRIP |
	    API_TERM_IFLAG_IXON);
	raw.oflag &= ~API_TERM_OFLAG_OPOST;
	raw.cflag = (raw.cflag & ~API_TERM_CFLAG_CSIZE) |
	    API_TERM_CFLAG_CS8;
	raw.lflag &= ~(API_TERM_LFLAG_ECHO | API_TERM_LFLAG_ECHOE |
	    API_TERM_LFLAG_ECHOK | API_TERM_LFLAG_ECHONL |
	    API_TERM_LFLAG_ECHOCTL | API_TERM_LFLAG_ECHOPRT |
	    API_TERM_LFLAG_ECHOKE | API_TERM_LFLAG_ICANON |
	    API_TERM_LFLAG_IEXTEN | API_TERM_LFLAG_ISIG);
	raw.cc[API_TERM_CC_VMIN] = 1;
	raw.cc[API_TERM_CC_VTIME] = 0;
	return (termSetMode(&raw));
}

int
termRestoreMode(const struct api_term_mode *saved)
{
	if (!saved) {
		errno = EINVAL;
		return (-1);
	}
	return (termSetMode(saved));
}
