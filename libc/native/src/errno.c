/* !DEFINES!

$define %type int as native errno value
$define %func __errno_location as function with args void
$define %func strerror as function with args int

*/

/* !SPACE!

$space %export __errno_location, strerror

*/

#include <errno.h>

static int	native_errno;

int *
__errno_location(void)
{
	return (&native_errno);
}

char *
strerror(int errnum)
{
	switch (errnum) {
	case EPERM:
		return ("Operation not permitted");
	case ENOENT:
		return ("No such file or directory");
	case ESRCH:
		return ("No such process");
	case EINTR:
		return ("Interrupted system call");
	case EIO:
		return ("Input/output error");
	case E2BIG:
		return ("Argument list too long");
	case ENOEXEC:
		return ("Exec format error");
	case EBADF:
		return ("Bad file descriptor");
	case ECHILD:
		return ("No child processes");
	case EAGAIN:
		return ("Resource temporarily unavailable");
	case ENOMEM:
		return ("Cannot allocate memory");
	case EACCES:
		return ("Permission denied");
	case EFAULT:
		return ("Bad address");
	case EINVAL:
		return ("Invalid argument");
	case EBUSY:
		return ("Device or resource busy");
	case EEXIST:
		return ("File exists");
	case EXDEV:
		return ("Invalid cross-device link");
	case ENODEV:
		return ("No such device");
	case ENOTDIR:
		return ("Not a directory");
	case EISDIR:
		return ("Is a directory");
	case EMFILE:
		return ("Too many open files");
	case ENOTTY:
		return ("Inappropriate device operation");
	case EFBIG:
		return ("File too large");
	case ENOSPC:
		return ("No space left on device");
	case ESPIPE:
		return ("Illegal seek");
	case EROFS:
		return ("Read-only file system");
	case EPIPE:
		return ("Broken pipe");
	case ENOSYS:
		return ("Function not implemented");
	case ENOTEMPTY:
		return ("Directory not empty");
	case EPROTO:
		return ("Protocol error");
	case EBADMSG:
		return ("Bad message");
	case EOVERFLOW:
		return ("Value too large");
	case ENOTSUP:
		return ("Operation not supported");
	case ETIMEDOUT:
		return ("Connection timed out");
	default:
		return ("Unknown error");
	}
}
