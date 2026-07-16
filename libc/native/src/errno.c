/* !DEFINES!

$define %type int as native errno value
$define %func __errno_location as function with args void

*/

/* !SPACE!

$space %export __errno_location

*/

#include <errno.h>

static int	native_errno;

int *
__errno_location(void)
{
	return (&native_errno);
}
