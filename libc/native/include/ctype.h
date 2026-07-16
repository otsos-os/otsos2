/* !DEFINES!

$define %type int as character classification input

*/

/* !SPACE!

$space %export isalpha, isdigit, isspace, isalnum, isxdigit
$space %export islower, isupper, tolower, toupper

*/

#ifndef _CTYPE_H
#define _CTYPE_H

int	isalpha(int c);
int	isdigit(int c);
int	isspace(int c);
int	isalnum(int c);
int	isxdigit(int c);
int	islower(int c);
int	isupper(int c);
int	tolower(int c);
int	toupper(int c);

#endif
