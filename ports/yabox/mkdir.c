/* !DEFINES!

$define %func mkdir_path as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal mkdir_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
mkdir_path(const char *path)
{
	int	code;

	if (dataDir(API_DATA_DIR_MKDIR, path, NULL) < 0) {
		code = errno;
		ybx_error("mkdir", path, code);
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
		fprintf(stderr, "usage: mkdir directory ...\n");
		return (1);
	}
	status = 0;
	for (i = 1; i < argc; i++) {
		if (mkdir_path(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
