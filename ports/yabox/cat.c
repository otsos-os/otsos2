/* !DEFINES!

$define %type FILE as native C stream
$define %func cat_file as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal cat_file
$space %export main

*/

#include <errno.h>
#include <stdio.h>
#include "yabox.h"

static int
cat_file(const char *path)
{
	char	buf[YBX_IO_BUFSIZE];
	FILE	*fp;
	size_t	n, w;
	int	code;

	fp = fopen(path, "r");
	if (!fp) {
		code = errno;
		ybx_error("cat", path, code);
		return (1);
	}
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
		w = fwrite(buf, 1, n, stdout);
		if (w != n) {
			code = errno;
			ybx_error("cat", "stdout", code);
			fclose(fp);
			return (1);
		}
	}
	if (ferror(fp)) {
		code = errno;
		ybx_error("cat", path, code);
		fclose(fp);
		return (1);
	}
	if (fclose(fp) != 0) {
		code = errno;
		ybx_error("cat", path, code);
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
		fprintf(stderr, "usage: cat file ...\n");
		return (1);
	}
	status = 0;
	for (i = 1; i < argc; i++) {
		if (cat_file(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
