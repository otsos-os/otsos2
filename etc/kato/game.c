/* !DEFINES!

$mode speed

$define %type int32 as 32 bit signed
$define %type char as 8 bit signed

$define %func putchar as function with args char
$define %func main as start with args void

*/

/* !SPACE!

$space %export main, putchar

*/

#include <stdbool.h>
#include <string.h>

void
putchar(char c);
int
main(void);

void
putchar(char c)
{
	__asm__ volatile (
		"mov $1, %%rax\n"
		"mov $1, %%rdi\n"
		"leaq %0, %%rsi\n"
		"mov $1, %%rdx\n"
		"syscall\n"
	 :
	 : "m"(c)
	 : "rax", "rdi", "rsi", "rdx", "rcx", "r11", "memory"
	);
	return;
}

int
main(void)
{
	putchar('\x68');
	putchar('\x65');
	putchar('\x6c');
	putchar('\x6c');
	putchar('\x6f');
	putchar('\x0a');
	return (0);
}
