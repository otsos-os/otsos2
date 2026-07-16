#include <native.h>
#include <stdio.h>

int
main(int argc, char **argv, char **envp)
{
	struct api_sysinfo	info;

	(void)argc;
	(void)argv;
	(void)envp;

	printf("hello from native libc\n");
	printf("pid=%d tid=%d\n", procGetpid(), procGettid());

	if (sysInfo(&info) == 0) {
		printf("%s %s %s\n", info.sysname, info.release,
		    info.machine);
	}
	return (0);
}
