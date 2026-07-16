/* !DEFINES!

$define %type bool as C boolean macro type

*/

/* !SPACE!

$space %export bool, true, false

*/

#ifndef _STDBOOL_H
#define _STDBOOL_H
#define bool	_Bool
#define true	1
#define false	0
#endif
