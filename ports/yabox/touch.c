/* !DEFINES!

$define %type int as native data handle
$define %func touch_path as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal touch_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
touch_path(const char *path)
{
	int	fd, code;

	fd = dataOpen(path, API_OPEN_WRITE | API_OPEN_CREATE);
	if (fd < 0) {
		code = errno;
		ybx_error("touch", path, code);
		return (1);
	}
	if (dataClose(fd) < 0) {
		code = errno;
		ybx_error("touch", path, code);
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
		fprintf(stderr, "usage: touch file ...\n");
		return (1);
	}
	status = 0;
	for (i = 1; i < argc; i++) {
		if (touch_path(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
