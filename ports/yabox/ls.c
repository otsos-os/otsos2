/* !DEFINES!

$define %type api_dirent as native directory entry
$define %type api_fs_stat as native file metadata
$define %func type_suffix as function with args unsigned int
$define %func ls_path as function with args const char *, int
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal type_suffix, ls_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include "yabox.h"

static char
type_suffix(unsigned int type)
{
	if (type == API_FS_TYPE_DIR) {
		return ('/');
	}
	if (type == API_FS_TYPE_CHR) {
		return ('*');
	}
	if (type == API_FS_TYPE_LNK) {
		return ('@');
	}
	return ('\0');
}

static int
ls_path(const char *path, int header)
{
	struct api_dirent	entries[YBX_DIRENT_MAX];
	struct api_fs_stat	st;
	char			suffix;
	const char		*show;
	int			n, i, code;

	show = (path && path[0]) ? path : ".";
	n = fsListdir(path, entries, YBX_DIRENT_MAX);
	if (n < 0) {
		code = errno;
		if (fsStat(path, &st) == 0 && st.type != API_FS_TYPE_DIR) {
			printf("%s\n", show);
			return (0);
		}
		ybx_error("ls", show, code);
		return (1);
	}
	if (header) {
		printf("%s:\n", show);
	}
	for (i = 0; i < n; i++) {
		suffix = type_suffix(entries[i].type);
		printf("%s", entries[i].name);
		if (suffix != '\0') {
			putchar(suffix);
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
		return (ls_path("", 0));
	}
	status = 0;
	multi = argc > 2;
	for (i = 1; i < argc; i++) {
		if (i > 1) {
			putchar('\n');
		}
		if (ls_path(argv[i], multi) != 0) {
			status = 1;
		}
	}
	return (status);
}
