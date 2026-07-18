/* !DEFINES!

$define %func move_one as function with args const char *, const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal move_one
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
move_one(const char *src, const char *dst)
{
	int	code;

	if (dataDir(API_DATA_DIR_RENAME, src, dst) < 0) {
		code = errno;
		ybx_error("mv", src, code);
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	char	target[YBX_PATH_MAX];
	const char	*dst;
	int		i, status, multi;

	(void)envp;
	if (argc < 3) {
		fprintf(stderr, "usage: mv source ... target\n");
		return (1);
	}
	dst = argv[argc - 1];
	multi = argc > 3;
	if (multi && !ybx_is_dir(dst)) {
		ybx_error("mv", dst, ENOTDIR);
		return (1);
	}
	status = 0;
	for (i = 1; i < argc - 1; i++) {
		if (ybx_target_path(target, sizeof(target), argv[i], dst,
		    multi) < 0) {
			ybx_error("mv", dst, errno);
			status = 1;
			continue;
		}
		if (move_one(argv[i], target) != 0) {
			status = 1;
		}
	}
	return (status);
}
