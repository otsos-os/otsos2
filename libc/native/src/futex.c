/* !DEFINES!

$define %type uint64_t as user address integer
$define %func futexWait as function with args uint64_t, uint32_t

*/

/* !SPACE!

$space %export futexWait, futexWake

*/

#include <native.h>
#include <stdint.h>
#include "private.h"

int
futexWait(uint64_t uaddr, uint32_t expected_val)
{
	return (__sysret_int(__syscall2(CALL_FUTEX_WAIT, (long)uaddr,
	    (long)expected_val)));
}

int
futexWake(uint64_t uaddr, uint32_t max_waiters)
{
	return (__sysret_int(__syscall2(CALL_FUTEX_WAKE, (long)uaddr,
	    (long)max_waiters)));
}
