/* !DEFINES!

$define %type api_key_event as struct with keyboard event data
$define %func inputRead as function with args api_key_event *, uint32_t, uint32_t

*/

/* !SPACE!

$space %export inputRead, inputPoll, inputFlush

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

int
inputRead(struct api_key_event *buf, uint32_t count, uint32_t flags)
{
	return (__sysret_int(__syscall3(CALL_INPUT_READ, (long)buf,
	    (long)count, (long)flags)));
}

int
inputPoll(void)
{
	return (__sysret_int(__syscall0(CALL_INPUT_POLL)));
}

int
inputFlush(void)
{
	return (__sysret_int(__syscall0(CALL_INPUT_FLUSH)));
}
