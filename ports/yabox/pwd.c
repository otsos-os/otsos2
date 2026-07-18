/* !DEFINES!

$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

int
main(int argc, char **argv, char **envp)
{
	char	buf[YBX_PATH_MAX];
	int	code;

	(void)argc;
	(void)argv;
	(void)envp;
	if (fsGetcwd(buf, sizeof(buf)) < 0) {
		code = errno;
		ybx_error("pwd", NULL, code);
		return (1);
	}
	printf("%s\n", buf);
	return (0);
}
