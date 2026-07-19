/* !DEFINES!

$define %type api_proc_info as native process entry
$define %type kevent as native event record
$define %func sleep_ms as procedure with args int
$define %func find_dhcpd as function with args uint32_t *, int *
$define %func check_dhcpd as function with args uint32_t, int
$define %func start_dhcpd as function with args void
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal sleep_ms, find_dhcpd, check_dhcpd, start_dhcpd
$space %export main

*/

#include <errno.h>
#include <native.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define	DHCPC_MAX_PROCS		64
#define	DHCPC_POLL_TRIES	40
#define	DHCPC_POLL_MS		50
#define	DHCPD_PATH		"/bin/dhcpd"
#define	PROC_STATE_ZOMBIE	5

static void
sleep_ms(int ms)
{
	struct kevent	change;
	struct kevent	event;
	int		kq;

	kq = eventKqueue();
	if (kq < 0) {
		return;
	}

	memset(&change, 0, sizeof(change));
	change.ident = 1;
	change.filter = EVFILT_TIMER;
	change.flags = EV_ADD | EV_ONESHOT;
	change.data = ms;
	(void)eventWait(kq, &change, 1, &event, 1, ms + 50);
	eventClose(kq);
}

static int
find_dhcpd(uint32_t *pid_out, int *perm_out)
{
	struct api_proc_info	procs[DHCPC_MAX_PROCS];
	int			n, i, perm;

	if (pid_out) {
		*pid_out = 0;
	}
	if (perm_out) {
		*perm_out = API_PROC_PERM_USER;
	}

	n = procList(procs, DHCPC_MAX_PROCS);
	if (n < 0) {
		return (-1);
	}

	for (i = 0; i < n; i++) {
		if (procs[i].state == PROC_STATE_ZOMBIE) {
			continue;
		}
		if (strcmp(procs[i].name, "dhcpd") != 0) {
			continue;
		}
		perm = procPerm(procs[i].pid);
		if (perm < 0) {
			return (-1);
		}
		if (pid_out) {
			*pid_out = procs[i].pid;
		}
		if (perm_out) {
			*perm_out = perm;
		}
		return (1);
	}

	return (0);
}

static int
check_dhcpd(uint32_t pid, int perm)
{
	if (perm != API_PROC_PERM_KUSR) {
		printf("dhcpc: dhcpd pid=%u is running without kusr\n",
		    (unsigned int)pid);
		printf("dhcpc: restart it after kusrAuth, then run dhcpc\n");
		return (1);
	}

	printf("dhcpc: dhcpd pid=%u is running with kusr\n",
	    (unsigned int)pid);
	return (0);
}

static int
start_dhcpd(void)
{
	char	*argv[2];
	int	status;
	int	pid;

	argv[0] = "dhcpd";
	argv[1] = NULL;

	pid = procSpawnNative(DHCPD_PATH, argv, NULL);
	if (pid < 0) {
		printf("dhcpc: spawn %s failed: %d\n", DHCPD_PATH, errno);
		return (-1);
	}

	(void)procWait(&status);
	return (0);
}

int
main(int argc, char **argv, char **envp)
{
	uint32_t	pid;
	int		perm;
	int		i, ret;

	(void)argc;
	(void)argv;
	(void)envp;
	personality(0);

	ret = find_dhcpd(&pid, &perm);
	if (ret < 0) {
		printf("dhcpc: procList failed: %d\n", errno);
		return (1);
	}
	if (ret > 0) {
		return (check_dhcpd(pid, perm));
	}

	printf("dhcpc: dhcpd is not running, starting it\n");
	if (start_dhcpd() != 0) {
		return (1);
	}

	for (i = 0; i < DHCPC_POLL_TRIES; i++) {
		ret = find_dhcpd(&pid, &perm);
		if (ret < 0) {
			printf("dhcpc: procList failed: %d\n", errno);
			return (1);
		}
		if (ret > 0) {
			return (check_dhcpd(pid, perm));
		}
		sleep_ms(DHCPC_POLL_MS);
	}

	printf("dhcpc: dhcpd did not stay running\n");
	return (1);
}
