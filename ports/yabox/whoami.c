/* !DEFINES!

$define %type uint32_t as 32 bit unsigned
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include "yabox.h"

int
main(int argc, char **argv, char **envp)
{
	uint32_t	pid;
	int		perm, code;

	(void)envp;
	pid = 0;
	if (argc > 2) {
		fprintf(stderr, "usage: whoami [pid]\n");
		return (1);
	}
	if (argc == 2 && ybx_parse_u32(argv[1], &pid) < 0) {
		ybx_error("whoami", argv[1], errno);
		return (1);
	}
	perm = procPerm(pid);
	if (perm < 0) {
		code = errno;
		ybx_error("whoami", argc == 2 ? argv[1] : NULL, code);
		return (1);
	}
	if (perm == API_PROC_PERM_KUSR) {
		printf("kusr\n");
	} else {
		printf("user\n");
	}
	return (0);
}
