/* !DEFINES!

$define %type FILE as native C stream
$define %type api_fs_stat as native file metadata
$define %func copy_one as function with args const char *, const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal copy_one
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdio.h>
#include "yabox.h"

static int
copy_one(const char *src, const char *dst)
{
	struct api_fs_stat	st;
	char			buf[YBX_IO_BUFSIZE];
	FILE			*in, *out;
	size_t			n, w;
	int			code;

	if (fsStat(src, &st) == 0 && st.type == API_FS_TYPE_DIR) {
		ybx_error("cp", src, EISDIR);
		return (1);
	}
	in = fopen(src, "r");
	if (!in) {
		code = errno;
		ybx_error("cp", src, code);
		return (1);
	}
	out = fopen(dst, "w");
	if (!out) {
		code = errno;
		ybx_error("cp", dst, code);
		fclose(in);
		return (1);
	}
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		w = fwrite(buf, 1, n, out);
		if (w != n) {
			code = errno;
			ybx_error("cp", dst, code);
			fclose(in);
			fclose(out);
			return (1);
		}
	}
	if (ferror(in)) {
		code = errno;
		ybx_error("cp", src, code);
		fclose(in);
		fclose(out);
		return (1);
	}
	if (fclose(in) != 0) {
		code = errno;
		ybx_error("cp", src, code);
		fclose(out);
		return (1);
	}
	if (fclose(out) != 0) {
		code = errno;
		ybx_error("cp", dst, code);
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
		fprintf(stderr, "usage: cp source ... target\n");
		return (1);
	}
	dst = argv[argc - 1];
	multi = argc > 3;
	if (multi && !ybx_is_dir(dst)) {
		ybx_error("cp", dst, ENOTDIR);
		return (1);
	}
	status = 0;
	for (i = 1; i < argc - 1; i++) {
		if (ybx_target_path(target, sizeof(target), argv[i], dst,
		    multi) < 0) {
			ybx_error("cp", dst, errno);
			status = 1;
			continue;
		}
		if (copy_one(argv[i], target) != 0) {
			status = 1;
		}
	}
	return (status);
}
