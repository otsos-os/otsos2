/* !DEFINES!

$define %func rmdir_path as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal rmdir_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
rmdir_path(const char *path)
{
	int	code;

	if (dataDir(API_DATA_DIR_RMDIR, path, NULL) < 0) {
		code = errno;
		ybx_error("rmdir", path, code);
		return (1);
	}
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	int	i, status;

	(void)envp;
	if (argc < 2) {
		fprintf(stderr, "usage: rmdir directory ...\n");
		return (1);
	}
	status = 0;
	for (i = 1; i < argc; i++) {
		if (rmdir_path(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
