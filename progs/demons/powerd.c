/*
 * Copyright (c) 2026, otsos team
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
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

$define %type kevent as native event descriptor
$define %func daemonize as function with args void
$define %func attach_power as function with args int
$define %func serve as function with args int
$define %func main as start with args int, char **, char **

*/

/* !SPACE!

$space %internal daemonize, attach_power, serve
$space %export main

*/

#include <native.h>
#include <stdint.h>
#include <string.h>

static int
daemonize(void)
{
	int	pid;

	pid = procCopy();
	if (pid < 0) {
		termPrint("powerd: first fork failed\n");
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	if (procSetsid() < 0) {
		termPrint("powerd: setsid failed\n");
		return (-1);
	}

	pid = procCopy();
	if (pid < 0) {
		termPrint("powerd: second fork failed\n");
		return (-1);
	}
	if (pid > 0) {
		procExit(0);
	}

	return (0);
}

static int
attach_power(int kq)
{
	struct kevent	change;

	memset(&change, 0, sizeof(change));
	change.ident = POWER_EVENT_IDENT_SYSTEM;
	change.filter = EVFILT_POWER;
	change.flags = EV_ADD | EV_CLEAR;
	return (eventWait(kq, &change, 1, NULL, 0, 0));
}

static int
serve(int kq)
{
	struct kevent	event;
	int		n;

	for (;;) {
		n = eventWait(kq, NULL, 0, &event, 1, -1);
		if (n < 0) {
			termPrint("powerd: event wait failed\n");
			(void)eventClose(kq);
			return (1);
		}
		if (n == 0 || event.filter != EVFILT_POWER ||
		    (event.fflags & NOTE_POWER_BUTTON) == 0) {
			continue;
		}

		if (powerState(API_POWER_STATE_SHUTDOWN) < 0) {
			termPrint("powerd: shutdown request failed\n");
			(void)eventClose(kq);
			return (1);
		}
	}
}

int
main(int argc, char **argv, char **envp)
{
	int	kq;

	(void)argc;
	(void)argv;
	(void)envp;
	(void)personality(API_PERSONALITY_NATIVE);

	if (procPerm(0) != API_PROC_PERM_KUSR) {
		termPrint("powerd: kusr privilege required\n");
		return (1);
	}
	if (daemonize() != 0) {
		return (1);
	}

	kq = eventKqueue();
	if (kq < 0) {
		termPrint("powerd: kqueue creation failed\n");
		return (1);
	}
	if (attach_power(kq) < 0) {
		termPrint("powerd: power event subscription failed\n");
		(void)eventClose(kq);
		return (1);
	}

	return (serve(kq));
}
