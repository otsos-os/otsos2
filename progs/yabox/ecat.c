/* !DEFINES!

$define %type api_entity_stat as native entity stat descriptor
$define %type uint64_t as 64 bit unsigned
$define %func ecat_resolve as function with args const char *, char *, size_t
$define %func ecat_path as function with args const char *
$define %func ecat_read_path as function with args const char *
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal ecat_resolve, ecat_path
$space %internal ecat_read_path
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "yabox.h"

static int
ecat_resolve(const char *path, char *out, size_t outsz)
{
	if (!path || !path[0]) {
		errno = EINVAL;
		return (-1);
	}
	if (strncmp(path, "/Entity", 7) == 0) {
		return (ybx_copy_path(out, outsz, path));
	}
	if (ybx_copy_path(out, outsz, "/Entity/") != 0) {
		return (-1);
	}
	if (strlen(out) + strlen(path) + 1 > outsz) {
		errno = E2BIG;
		return (-1);
	}
	strcat(out, path);
	return (0);
}

static int
ecat_path(const char *path)
{
	struct api_entity_stat	st;
	char			full[YBX_PATH_MAX];
	uint64_t		value;
	int32_t			ivalue;
	int			handle;
	int			i;
	int			ret;

	if (ecat_resolve(path, full, sizeof(full)) != 0) {
		ybx_error("ecat", path, errno);
		return (1);
	}
	handle = entityOpen(full, ENTITY_ACCESS_READ);
	if (handle < 0) {
		ybx_error("ecat", full, errno);
		return (1);
	}
	ret = entityStat(handle, &st);
	if (ret != 0) {
		ybx_error("ecat", full, errno);
		entityClose(handle);
		return (1);
	}

	printf("name:      %s\n", st.name);
	printf("id:        %llu\n", (unsigned long long)st.id);
	printf("archetype: %u\n", (unsigned int)st.archetype);
	printf("state:     %u\n", (unsigned int)st.state);
	printf("flags:     %u\n", (unsigned int)st.flags);
	printf("refs:      %d\n", (int)st.refs);
	printf("owner:     %u\n", (unsigned int)st.owner_pid);
	printf("uid/gid:   %u/%u\n", (unsigned int)st.uid,
	    (unsigned int)st.gid);
	printf("euid/egid: %u/%u\n", (unsigned int)st.euid,
	    (unsigned int)st.egid);
	printf("size:      %llu\n", (unsigned long long)st.size);
	printf("created:   %llu\n", (unsigned long long)st.created);

	printf("data:\n");
	for (i = 0; i < 8; i++) {
		if (entityGetData(handle, (uint32_t)i, &value) == 0) {
			printf("  [%d] = %llu\n", i,
			    (unsigned long long)value);
		}
	}
	printf("i32:\n");
	for (i = 0; i < 8; i++) {
		if (entityGetI32(handle, (uint32_t)i, &ivalue) == 0) {
			printf("  [%d] = %d\n", i, (int)ivalue);
		}
	}

	entityClose(handle);
	return (0);
}

static int
ecat_read_path(const char *path)
{
	char			buf[256];
	char			full[YBX_PATH_MAX];
	ssize_t			n;
	int			handle;

	if (ecat_resolve(path, full, sizeof(full)) != 0) {
		ybx_error("ecat", path, errno);
		return (1);
	}
	handle = entityOpen(full, ENTITY_ACCESS_READ);
	if (handle < 0) {
		ybx_error("ecat", full, errno);
		return (1);
	}
	for (;;) {
		n = entityRead(handle, buf, sizeof(buf) - 1);
		if (n < 0) {
			ybx_error("ecat", full, errno);
			entityClose(handle);
			return (1);
		}
		if (n == 0) {
			break;
		}
		buf[n] = '\0';
		printf("%s", buf);
	}
	entityClose(handle);
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	int	i, read_mode, status;

	(void)envp;
	if (argc < 2) {
		fprintf(stderr, "usage: ecat [-r] <entity-path>\n");
		return (1);
	}
	read_mode = 0;
	i = 1;
	if (strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--read") == 0) {
		read_mode = 1;
		i = 2;
	}
	if (i >= argc) {
		fprintf(stderr, "usage: ecat [-r] <entity-path>\n");
		return (1);
	}
	status = 0;
	for (; i < argc; i++) {
		if (read_mode ? ecat_read_path(argv[i]) != 0 :
		    ecat_path(argv[i]) != 0) {
			status = 1;
		}
	}
	return (status);
}
