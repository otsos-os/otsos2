/* !DEFINES!

$define %type char ** as process argument vector
$define %func __libc_start as start with args long, char **, char **

*/

/* !SPACE!

$space %export __libc_start, environ

*/

#include <stdlib.h>

extern int	main(int argc, char **argv, char **envp);

char	**environ;

static char	*empty_argv_storage[1];

void
__libc_start(long argc, char **argv, char **envp)
{
	int	ret;

	if (argc < 0) {
		argc = 0;
	}
	if (!argv) {
		argv = empty_argv_storage;
		argv[0] = 0;
		argc = 0;
	}
	if (!envp) {
		envp = empty_argv_storage;
	}
	environ = envp;

	ret = main((int)argc, argv, envp);
	exit(ret);
}
