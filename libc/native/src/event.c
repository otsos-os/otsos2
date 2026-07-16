/* !DEFINES!

$define %type kevent_args as native event syscall argument block
$define %type kevent as struct with native event data
$define %func eventWait as function with args int, kevent *, int, kevent *, int, int64_t

*/

/* !SPACE!

$space %export eventKqueue, eventClose, eventWait

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

struct kevent_args {
	int		kq_idx;
	struct kevent	*changelist;
	int		nchanges;
	struct kevent	*eventlist;
	int		nevents;
	int64_t		timeout_ms;
};

int
eventKqueue(void)
{
	return (__sysret_int(__syscall0(CALL_EVENT_KQUEUE)));
}

int
eventClose(int kq)
{
	return (__sysret_int(__syscall1(CALL_EVENT_CLOSE, (long)kq)));
}

int
eventWait(int kq, struct kevent *changes, int nchanges,
    struct kevent *events, int nevents, int64_t timeout_ms)
{
	struct kevent_args	args;

	args.kq_idx = kq;
	args.changelist = changes;
	args.nchanges = nchanges;
	args.eventlist = events;
	args.nevents = nevents;
	args.timeout_ms = timeout_ms;

	return (__sysret_int(__syscall3(CALL_EVENT_KEVENT, 0,
	    (long)&args, 0)));
}
