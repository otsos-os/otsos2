/* !DEFINES!

$define %type api_entity_entry as native entity list entry
$define %func lse_path as function with args const char *, int
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal lse_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "yabox.h"

static int
lse_path(const char *path, int header)
{
	struct api_entity_entry	entries[YBX_DIRENT_MAX];
	char			full[YBX_PATH_MAX];
	const char		*show;
	int			n, i, code;

	if (!path || !path[0]) {
		show = "/Entity";
	} else if (strncmp(path, "/Entity", 7) == 0) {
		show = path;
	} else {
		if (ybx_copy_path(full, sizeof(full), "/Entity/") != 0) {
			ybx_error("lse", path, E2BIG);
			return (1);
		}
		if (strlen(full) + strlen(path) + 1 > sizeof(full)) {
			ybx_error("lse", path, E2BIG);
			return (1);
		}
		strcat(full, path);
		show = full;
	}
	n = entityList(show, entries, YBX_DIRENT_MAX);
	if (n < 0) {
		code = errno;
		ybx_error("lse", show, code);
		return (1);
	}
	if (header) {
		printf("%s:\n", show);
	}
	if (n == 0) {
		printf("(empty)\n");
		return (0);
	}
	for (i = 0; i < n; i++) {
		printf("%s", entries[i].name);
		if (entries[i].id == 0) {
			putchar('/');
		}
		if (i + 1 < n) {
			printf("  ");
		}
	}
	putchar('\n');
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	int	i, status, multi;

	(void)envp;
	if (argc < 2) {
		return (lse_path("/Entity", 0));
	}
	status = 0;
	multi = argc > 2;
	for (i = 1; i < argc; i++) {
		if (i > 1) {
			putchar('\n');
		}
		if (lse_path(argv[i], multi) != 0) {
			status = 1;
		}
	}
	return (status);
}
