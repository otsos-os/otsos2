/* !DEFINES!

$define %type api_fs_stat as struct with native file metadata
$define %type api_dirent as struct with native directory entry
$define %func fsStat as function with args const char *, api_fs_stat *

*/

/* !SPACE!

$space %export fsChdir, fsGetcwd, fsListdir, fsStat, fsRename, fsUnlink
$space %export fsLinkNew, fsLinkGo

*/

#include <native.h>
#include <stddef.h>
#include <stdint.h>
#include "private.h"

int
fsChdir(const char *path)
{
	return (__sysret_int(__syscall1(CALL_FS_CHDIR, (long)path)));
}

int
fsGetcwd(char *buf, size_t size)
{
	if (!__count_ok(size)) {
		return (-1);
	}
	return (__sysret_int(__syscall2(CALL_FS_GETCWD, (long)buf,
	    (long)size)));
}

int
fsListdir(const char *path, struct api_dirent *buf, uint32_t max_entries)
{
	return (__sysret_int(__syscall3(CALL_FS_LISTDIR, (long)path,
	    (long)buf, (long)max_entries)));
}

int
fsStat(const char *path, struct api_fs_stat *buf)
{
	return (__sysret_int(__syscall2(CALL_FS_STAT, (long)path,
	    (long)buf)));
}

int
fsRename(const char *oldpath, const char *newpath)
{
	return (__sysret_int(__syscall2(CALL_FS_RENAME, (long)oldpath,
	    (long)newpath)));
}

int
fsUnlink(const char *path)
{
	return (__sysret_int(__syscall1(CALL_FS_UNLINK, (long)path)));
}

int
fsLinkNew(const char *target, const char *linkpath, uint32_t flags)
{
	return (__sysret_int(__syscall3(CALL_FS_LINKNEW, (long)target,
	    (long)linkpath, (long)flags)));
}

int
fsLinkGo(const char *path, char *buf, uint32_t bufsize)
{
	return (__sysret_int(__syscall3(CALL_FS_LINKGO, (long)path,
	    (long)buf, (long)bufsize)));
}
