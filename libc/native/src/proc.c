/* !DEFINES!

$define %type api_proc_info as struct with process table entry
$define %func procSpawn as function with args const char *, char *const *, char *const *
$define %func procSpawnAbi as function with args const char *, char *const *, char *const *, uint32_t
$define %func procSpawnPty as function with args const char *, char *const *, char *const *, int, uint32_t
$define %func procPerm as function with args uint32_t
$define %func procExit as procedure with args int

*/

/* !SPACE!

$space %export procClone, procCopy, procSpawn, procSpawnAbi, procSpawnNative
$space %export procSpawnPty
$space %export procWait, procTryWait, procRun, procExit
$space %export procKill, procList, procGetpid, procGetppid, procGettid
$space %export procPerm
$space %export threadExit, threadJoin, procExitGroup, procSetTidAddress
$space %export procSetsid, procGetsid, kusrAuth, personality

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

long
procClone(uint64_t flags, void *child_stack, uint64_t ptid)
{
	return (__sysret(__syscall3(CALL_PROC_CLONE, (long)flags,
	    (long)child_stack, (long)ptid)));
}

int
procCopy(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_COPY)));
}

int
procSpawnAbi(const char *path, char *const argv[], char *const envp[],
    uint32_t abi)
{
	struct api_proc_spawn_args	args;

	args.size = sizeof(args);
	args.flags = 0;
	args.abi = abi;
	args.tty = 0;
	args.path = path;
	args.argv = (const char *const *)argv;
	args.envp = (const char *const *)envp;
	return (__sysret_int(__syscall1(CALL_PROC_SPAWN, (long)&args)));
}

int
procSpawn(const char *path, char *const argv[], char *const envp[])
{
	return (procSpawnAbi(path, argv, envp, API_PROC_SPAWN_ABI_POSIX));
}

int
procSpawnNative(const char *path, char *const argv[], char *const envp[])
{
	return (procSpawnAbi(path, argv, envp, API_PROC_SPAWN_ABI_NATIVE));
}

int
procSpawnPty(const char *path, char *const argv[], char *const envp[],
    int pts_id, uint32_t abi)
{
	struct api_proc_spawn_args	args;

	args.size = sizeof(args);
	args.flags = API_PROC_SPAWN_SET_TTY;
	args.abi = abi;
	args.tty = -2 - (pts_id + 1);
	args.path = path;
	args.argv = (const char *const *)argv;
	args.envp = (const char *const *)envp;
	return (__sysret_int(__syscall1(CALL_PROC_SPAWN, (long)&args)));
}

int
procWait(int *status)
{
	return (__sysret_int(__syscall1(CALL_PROC_WAIT, (long)status)));
}

int
procTryWait(int *status)
{
	return (__sysret_int(__syscall1(CALL_PROC_TRYWAIT, (long)status)));
}

int
procRun(const char *path, char *const argv[], char *const envp[], int *status)
{
	int	pid;
	int	got;

	pid = procSpawn(path, argv, envp);
	if (pid < 0) {
		return (-1);
	}
	got = procWait(status);
	if (got < 0) {
		return (-1);
	}
	return (got);
}

void
procExit(int code)
{
	__syscall1(CALL_PROC_EXIT, (long)code);
	for (;;) {
	}
}

int
procKill(uint32_t pid, int sig)
{
	return (__sysret_int(__syscall2(CALL_PROC_KILL, (long)pid,
	    (long)sig)));
}

int
procList(struct api_proc_info *buf, uint32_t max_entries)
{
	return (__sysret_int(__syscall2(CALL_PROC_LIST, (long)buf,
	    (long)max_entries)));
}

int
procGetpid(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_GETPID)));
}

int
procGetppid(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_GETPPID)));
}

int
procGettid(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_GETTID)));
}
int
procPerm(uint32_t pid)
{
	return (__sysret_int(__syscall1(CALL_PROC_PERM, (long)pid)));
}

void
threadExit(int code)
{
	__syscall1(CALL_PROC_THREAD_EXIT, (long)code);
	for (;;) {
	}
}

int
threadJoin(uint32_t tid, int *status)
{
	return (__sysret_int(__syscall2(CALL_PROC_THREAD_JOIN, (long)tid,
	    (long)status)));
}

void
procExitGroup(int code)
{
	__syscall1(CALL_PROC_EXIT_GROUP, (long)code);
	for (;;) {
	}
}

int
procSetTidAddress(uint64_t tidptr)
{
	return (__sysret_int(__syscall1(CALL_PROC_SET_TID_ADDR,
	    (long)tidptr)));
}

int
procSetsid(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_SETSID)));
}

int
procGetsid(void)
{
	return (__sysret_int(__syscall0(CALL_PROC_GETSID)));
}

int
kusrAuth(const char *password)
{
	return (__sysret_int(__syscall1(CALL_KUSR_AUTH, (long)password)));
}

long
personality(long mode)
{
	return (__sysret(__syscall1(CALL_PERSONALITY, mode)));
}
