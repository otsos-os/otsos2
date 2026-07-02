/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* !DEFINES!

$define %type int as 32 bit signed
$define %type size_t as unsigned long
$define %type char as 8 bit signed

$define %func main as start with args int, char **

*/

/* !SPACE!

$space %export main

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int
main(int argc, char *argv[])
{
	char	buf[128];
	int	*arr;
	int	*arr2;
	char	*str;
	int	pid;
	int	i;

	pid = (int)getpid();

	printf("hello\n");
	printf("pid=%d argc=%d\n", pid, argc);

	if (argc > 1) {
		printf("argv=%s\n", argv[1]);
	}

	snprintf(buf, sizeof(buf), "argv[0] strlen = %zu\n",
	    strlen(argv[0]));
	write(1, buf, strlen(buf));

	printf("memory: malloc test\n");
	arr = malloc(10 * sizeof(int));
	if (!arr) {
		printf("memory: malloc failed\n");
		return (1);
	}
	for (i = 0; i < 10; i++) {
		arr[i] = i * i;
	}
	printf("memory: arr[9] = %d\n", arr[9]);

	printf("memory: realloc test\n");
	arr2 = realloc(arr, 20 * sizeof(int));
	if (!arr2) {
		printf("memory: realloc failed\n");
		free(arr);
		return (1);
	}
	arr2[19] = 12345;
	printf("memory: arr2[19] = %d\n", arr2[19]);
	free(arr2);

	printf("memory: calloc test\n");
	str = calloc(64, 1);
	if (!str) {
		printf("memory: calloc failed\n");
		return (1);
	}
	str[0] = 'o';
	str[1] = 'k';
	printf("memory: calloc = %s\n", str);
	free(str);

	printf("memory: done\n");

	if (argc > 1 && strcmp(argv[1], "exec_child") == 0) {
		printf("exec_child: running after execve\n");
		return (0);
	}

	printf("fork: test\n");
	{
		pid_t	child;
		int	status;
		char	*args[] = { "/bin/musl_test", "exec_child", NULL };
		char	*envp[] = { NULL };

		child = fork();
		if (child < 0) {
			printf("fork: fork failed\n");
			return (1);
		}
		if (child == 0) {
			printf("fork: child pid=%d\n", getpid());
			execve("/bin/musl_test", args, envp);
			printf("fork: execve failed\n");
			return (1);
		}
		printf("fork: parent, child=%d\n", child);
		if (waitpid(child, &status, 0) != child) {
			printf("fork: waitpid failed\n");
			return (1);
		}
		if (!WIFEXITED(status)) {
			printf("fork: child did not exit cleanly\n");
			return (1);
		}
		printf("fork: child exited with %d\n",
		    WEXITSTATUS(status));
		if (WEXITSTATUS(status) != 0) {
			printf("fork: child exit code non-zero\n");
			return (1);
		}
	}
	printf("fork: done\n");

	return (0);
}
