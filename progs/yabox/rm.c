/* !DEFINES!

$define %func rm_path as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal rm_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
rm_path(const char *path)
{
	int	code;

	if (fsUnlink(path) < 0) {
		code = errno;
		ybx_error("rm", path, code);
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
		fprintf(stderr, "usage: rm file ...\n");
		return (1);
	}
	status = 0;
	for (i = 1; i < argc; i++) {
		if (rm_path(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
